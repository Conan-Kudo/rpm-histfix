/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2003
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char copyright[] =
    "Copyright (c) 1996-2003\nSleepycat Software Inc.  All rights reserved.\n";
static const char revid[] =
    "$Id: db_printlog.c,v 11.59 2003/08/18 18:00:31 ubell Exp $";
#endif

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/btree.h"
#include "dbinc/fop.h"
#include "dbinc/hash.h"
#include "dbinc/log.h"
#include "dbinc/qam.h"
#include "dbinc/txn.h"

int db_printlog_main __P((int, char *[]));
int db_printlog_usage __P((void));
int db_printlog_version_check __P((const char *));
int db_printlog_print_app_record __P((DB_ENV *, DBT *, DB_LSN *, db_recops));
int db_printlog_open_rep_db __P((DB_ENV *, DB **, DBC **));

int
db_printlog(args)
	char *args;
{
	int argc;
	char **argv;

	__db_util_arg("db_printlog", args, &argc, &argv);
	return (db_printlog_main(argc, argv) ? EXIT_FAILURE : EXIT_SUCCESS);
}

#include <stdio.h>
#define	ERROR_RETURN	ERROR

int
db_printlog_main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int optind, __db_getopt_reset;
	const char *progname = "db_printlog";
	DB *dbp;
	DBC *dbc;
	DB_ENV	*dbenv;
	DB_LOGC *logc;
	int (**dtab) __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
	size_t dtabsize;
	DBT data, keydbt;
	DB_LSN key;
	int ch, exitval, nflag, rflag, ret, repflag;
	char *home, *passwd;

	if ((ret = db_printlog_version_check(progname)) != 0)
		return (ret);

	dbenv = NULL;
	dbp = NULL;
	dbc = NULL;
	logc = NULL;
	exitval = nflag = rflag = repflag = 0;
	home = passwd = NULL;
	dtabsize = 0;
	dtab = NULL;
	__db_getopt_reset = 1;
	while ((ch = getopt(argc, argv, "h:NP:rRV")) != EOF)
		switch (ch) {
		case 'h':
			home = optarg;
			break;
		case 'N':
			nflag = 1;
			break;
		case 'P':
			passwd = strdup(optarg);
			memset(optarg, 0, strlen(optarg));
			if (passwd == NULL) {
				fprintf(stderr, "%s: strdup: %s\n",
				    progname, strerror(errno));
				return (EXIT_FAILURE);
			}
			break;
		case 'r':
			rflag = 1;
			break;
		case 'R':
			repflag = 1;
			break;
		case 'V':
			printf("%s\n", db_version(NULL, NULL, NULL));
			return (EXIT_SUCCESS);
		case '?':
		default:
			return (db_printlog_usage());
		}
	argc -= optind;
	argv += optind;

	if (argc > 0)
		return (db_printlog_usage());

	/* Handle possible interruptions. */
	__db_util_siginit();

	/*
	 * Create an environment object and initialize it for error
	 * reporting.
	 */
	if ((ret = db_env_create(&dbenv, 0)) != 0) {
		fprintf(stderr,
		    "%s: db_env_create: %s\n", progname, db_strerror(ret));
		goto shutdown;
	}

	dbenv->set_errfile(dbenv, stderr);
	dbenv->set_errpfx(dbenv, progname);

	if (nflag) {
		if ((ret = dbenv->set_flags(dbenv, DB_NOLOCKING, 1)) != 0) {
			dbenv->err(dbenv, ret, "set_flags: DB_NOLOCKING");
			goto shutdown;
		}
		if ((ret = dbenv->set_flags(dbenv, DB_NOPANIC, 1)) != 0) {
			dbenv->err(dbenv, ret, "set_flags: DB_NOPANIC");
			goto shutdown;
		}
	}

	if (passwd != NULL && (ret = dbenv->set_encrypt(dbenv,
	    passwd, DB_ENCRYPT_AES)) != 0) {
		dbenv->err(dbenv, ret, "set_passwd");
		goto shutdown;
	}

	/*
	 * Set up an app-specific dispatch function so that we can gracefully
	 * handle app-specific log records.
	 */
	if ((ret = dbenv->set_app_dispatch(dbenv, db_printlog_print_app_record)) != 0) {
		dbenv->err(dbenv, ret, "app_dispatch");
		goto shutdown;
	}

	/*
	 * An environment is required, but as all we're doing is reading log
	 * files, we create one if it doesn't already exist.  If we create
	 * it, create it private so it automatically goes away when we're done.
	 * If we are reading the replication database, do not open the env
	 * with logging, because we don't want to log the opens.
	 */
	if (repflag) {
		if ((ret = dbenv->open(dbenv, home,
		    DB_INIT_MPOOL | DB_USE_ENVIRON, 0)) != 0 &&
		    (ret = dbenv->open(dbenv, home,
		    DB_CREATE | DB_INIT_MPOOL | DB_PRIVATE | DB_USE_ENVIRON, 0))
		    != 0) {
			dbenv->err(dbenv, ret, "open");
			goto shutdown;
		}
	} else if ((ret = dbenv->open(dbenv, home,
	    DB_JOINENV | DB_USE_ENVIRON, 0)) != 0 &&
	    (ret = dbenv->open(dbenv, home,
	    DB_CREATE | DB_INIT_LOG | DB_PRIVATE | DB_USE_ENVIRON, 0)) != 0) {
		dbenv->err(dbenv, ret, "open");
		goto shutdown;
	}

	/* Initialize print callbacks. */
	if ((ret = __bam_init_print(dbenv, &dtab, &dtabsize)) != 0 ||
	    (ret = __dbreg_init_print(dbenv, &dtab, &dtabsize)) != 0 ||
	    (ret = __crdel_init_print(dbenv, &dtab, &dtabsize)) != 0 ||
	    (ret = __db_init_print(dbenv, &dtab, &dtabsize)) != 0 ||
	    (ret = __fop_init_print(dbenv, &dtab, &dtabsize)) != 0 ||
	    (ret = __qam_init_print(dbenv, &dtab, &dtabsize)) != 0 ||
	    (ret = __ham_init_print(dbenv, &dtab, &dtabsize)) != 0 ||
	    (ret = __txn_init_print(dbenv, &dtab, &dtabsize)) != 0) {
		dbenv->err(dbenv, ret, "callback: initialization");
		goto shutdown;
	}

	/* Allocate a log cursor. */
	if (repflag) {
		if ((ret = db_printlog_open_rep_db(dbenv, &dbp, &dbc)) != 0)
			goto shutdown;
	} else if ((ret = dbenv->log_cursor(dbenv, &logc, 0)) != 0) {
		dbenv->err(dbenv, ret, "DB_ENV->log_cursor");
		goto shutdown;
	}

	memset(&data, 0, sizeof(data));
	memset(&keydbt, 0, sizeof(keydbt));
	while (!__db_util_interrupted()) {
		if (repflag) {
			ret = dbc->c_get(dbc,
			    &keydbt, &data, rflag ? DB_PREV : DB_NEXT);
			if (ret == 0)
				key = ((REP_CONTROL *)keydbt.data)->lsn;
		} else
			ret = logc->get(logc,
			    &key, &data, rflag ? DB_PREV : DB_NEXT);
		if (ret != 0) {
			if (ret == DB_NOTFOUND)
				break;
			dbenv->err(dbenv,
			    ret, repflag ? "DB_LOGC->get" : "DBC->get");
			goto shutdown;
		}

		ret = __db_dispatch(dbenv,
		    dtab, dtabsize, &data, &key, DB_TXN_PRINT, NULL);

		/*
		 * XXX
		 * Just in case the underlying routines don't flush.
		 */
		(void)fflush(stdout);

		if (ret != 0) {
			dbenv->err(dbenv, ret, "tx: dispatch");
			goto shutdown;
		}
	}

	if (0) {
shutdown:	exitval = 1;
	}
	if (logc != NULL && (ret = logc->close(logc, 0)) != 0)
		exitval = 1;

	if (dbc != NULL && (ret = dbc->c_close(dbc)) != 0)
		exitval = 1;

	if (dbp != NULL && (ret = dbp->close(dbp, 0)) != 0)
		exitval = 1;

	/*
	 * The dtab is allocated by __db_add_recovery (called by *_init_print)
	 * using the library malloc function (__os_malloc).  It thus needs to be
	 * freed using the corresponding free (__os_free).
	 */
	if (dtab != NULL)
		__os_free(dbenv, dtab);
	if (dbenv != NULL && (ret = dbenv->close(dbenv, 0)) != 0) {
		exitval = 1;
		fprintf(stderr,
		    "%s: dbenv->close: %s\n", progname, db_strerror(ret));
	}

	if (passwd != NULL)
		free(passwd);

	/* Resend any caught signal. */
	__db_util_sigresend();

	return (exitval == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}

int
db_printlog_usage()
{
	fprintf(stderr, "%s\n",
	    "usage: db_printlog [-NrV] [-h home] [-P password]");
	return (EXIT_FAILURE);
}

int
db_printlog_version_check(progname)
	const char *progname;
{
	int v_major, v_minor, v_patch;

	/* Make sure we're loaded with the right version of the DB library. */
	(void)db_version(&v_major, &v_minor, &v_patch);
	if (v_major != DB_VERSION_MAJOR || v_minor != DB_VERSION_MINOR) {
		fprintf(stderr,
	"%s: version %d.%d doesn't match library version %d.%d\n",
		    progname, DB_VERSION_MAJOR, DB_VERSION_MINOR,
		    v_major, v_minor);
		return (EXIT_FAILURE);
	}
	return (0);
}

/* Print an unknown, application-specific log record as best we can. */
int
db_printlog_print_app_record(dbenv, dbt, lsnp, op)
	DB_ENV *dbenv;
	DBT *dbt;
	DB_LSN *lsnp;
	db_recops op;
{
	int ch;
	u_int32_t i, rectype;

	DB_ASSERT(op == DB_TXN_PRINT);

	COMPQUIET(dbenv, NULL);
	COMPQUIET(op, DB_TXN_PRINT);

	/*
	 * Fetch the rectype, which always must be at the beginning of the
	 * record (if dispatching is to work at all).
	 */
	memcpy(&rectype, dbt->data, sizeof(rectype));

	/*
	 * Applications may wish to customize the output here based on the
	 * rectype.  We just print the entire log record in the generic
	 * mixed-hex-and-printable format we use for binary data.
	 */
	printf("[%lu][%lu]application specific record: rec: %lu\n",
	    (u_long)lsnp->file, (u_long)lsnp->offset, (u_long)rectype);
	printf("\tdata: ");
	for (i = 0; i < dbt->size; i++) {
		ch = ((u_int8_t *)dbt->data)[i];
		printf(isprint(ch) || ch == 0x0a ? "%c" : "%#x ", ch);
	}
	printf("\n\n");

	return (0);
}

int
db_printlog_open_rep_db(dbenv, dbpp, dbcp)
	DB_ENV *dbenv;
	DB **dbpp;
	DBC **dbcp;
{
	int ret;

	DB *dbp;
	*dbpp = NULL;
	*dbcp = NULL;

	if ((ret = db_create(dbpp, dbenv, 0)) != 0) {
		dbenv->err(dbenv, ret, "db_create");
		return (ret);
	}

	dbp = *dbpp;
	if ((ret =
	    dbp->open(dbp, NULL, "__db.rep.db", NULL, DB_BTREE, 0, 0)) != 0) {
		dbenv->err(dbenv, ret, "DB->open");
		goto err;
	}

	if ((ret = dbp->cursor(dbp, NULL, dbcp, 0)) != 0) {
		dbenv->err(dbenv, ret, "DB->cursor");
		goto err;
	}

	return (0);

err:	if (*dbpp != NULL)
		(void)(*dbpp)->close(*dbpp, 0);
	return (ret);
}
