/*
 * $Id: inmem.c,v 1.12 2007/06/22 15:21:24 bostic Exp $
 */

#include "bench.h"

/*
 * The in-memory tests don't run on early releases of Berkeley DB.
 */
#if DB_VERSION_MAJOR > 4 || DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR > 1

#undef	MEGABYTE
#define	MEGABYTE	(1024 * 1024)

u_int32_t bulkbufsize = 4 * MEGABYTE;
u_int32_t cachesize = 32 * MEGABYTE;
u_int32_t datasize = 32;
u_int32_t keysize = 8;
u_int32_t logbufsize = 8 * MEGABYTE;
u_int32_t numitems;
u_int32_t pagesize = 32 * 1024;

FILE *fp;
char *progname;

void op_ds(u_int, int);
void op_ds_bulk(u_int, u_int *);
void op_tds(u_int, int, u_int32_t);
int  usage __P((void));

void
op_ds(u_int ops, int update)
{
	char *letters = "abcdefghijklmnopqrstuvwxuz";
	DB *dbp;
	DBT key, data;
	char *keybuf, *databuf;
	DB_MPOOL_STAT  *gsp;

	DB_BENCH_ASSERT((keybuf = malloc(keysize)) != NULL);
	DB_BENCH_ASSERT((databuf = malloc(datasize)) != NULL);

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));
	key.data = keybuf;
	key.size = keysize;
	memset(keybuf, 'a', keysize);

	data.data = databuf;
	data.size = datasize;
	memset(databuf, 'b', datasize);

	DB_BENCH_ASSERT(db_create(&dbp, NULL, 0) == 0);

	dbp->set_errfile(dbp, stderr);

	DB_BENCH_ASSERT(dbp->set_pagesize(dbp, pagesize) == 0);
	DB_BENCH_ASSERT(dbp->open(
	    dbp, NULL, NULL, NULL, DB_BTREE, DB_CREATE, 0666) == 0);

	dbp->dbenv->memp_stat(dbp->dbenv, &gsp, NULL, DB_STAT_CLEAR);

	if (update) {
		TIMER_START;
		for (; ops > 0; --ops) {
			keybuf[(ops % keysize)] = letters[(ops % 26)];
			DB_BENCH_ASSERT(
			    dbp->put(dbp, NULL, &key, &data, 0) == 0);
		}
		TIMER_STOP;
	} else {
		DB_BENCH_ASSERT(dbp->put(dbp, NULL, &key, &data, 0) == 0);
		TIMER_START;
		for (; ops > 0; --ops)
			DB_BENCH_ASSERT(
			    dbp->get(dbp, NULL, &key, &data, 0) == 0);
		TIMER_STOP;
	}

	dbp->dbenv->memp_stat(dbp->dbenv, &gsp, NULL, 0);
	DB_BENCH_ASSERT(gsp->st_cache_miss == 0);

	DB_BENCH_ASSERT(dbp->close(dbp, 0) == 0);
}

void
op_ds_bulk(u_int ops, u_int *totalp)
{
	DB *dbp;
	DBC *dbc;
	DBT key, data;
	u_int32_t len, klen;
	u_int i, total;
	char *keybuf, *databuf;
	void *pointer, *dp, *kp;
	DB_MPOOL_STAT  *gsp;

	DB_BENCH_ASSERT((keybuf = malloc(keysize)) != NULL);
	DB_BENCH_ASSERT((databuf = malloc(bulkbufsize)) != NULL);

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));
	key.data = keybuf;
	key.size = keysize;

	data.data = databuf;
	data.size = datasize;
	memset(databuf, 'b', datasize);

	DB_BENCH_ASSERT(db_create(&dbp, NULL, 0) == 0);

	dbp->set_errfile(dbp, stderr);

	DB_BENCH_ASSERT(dbp->set_pagesize(dbp, pagesize) == 0);
	DB_BENCH_ASSERT(dbp->set_cachesize(dbp, 0, cachesize, 1) == 0);
	DB_BENCH_ASSERT(
	    dbp->open(dbp, NULL, NULL, NULL, DB_BTREE, DB_CREATE, 0666) == 0);

	for (i = 1; i <= numitems; ++i) {
		(void)sprintf(keybuf, "%10d", i);
		DB_BENCH_ASSERT(dbp->put(dbp, NULL, &key, &data, 0) == 0);
	}

#if 0
	fp = fopen("before", "w");
	dbp->set_msgfile(dbp, fp);
	DB_BENCH_ASSERT (dbp->stat_print(dbp, DB_STAT_ALL) == 0);
#endif

	DB_BENCH_ASSERT(dbp->cursor(dbp, NULL, &dbc, 0) == 0);

	data.ulen = bulkbufsize;
	data.flags = DB_DBT_USERMEM;

	dbp->dbenv->memp_stat(dbp->dbenv, &gsp, NULL, DB_STAT_CLEAR);

	TIMER_START;
	for (total = 0; ops > 0; --ops) {
		DB_BENCH_ASSERT(dbc->c_get(
		    dbc, &key, &data, DB_FIRST | DB_MULTIPLE_KEY) == 0);
		DB_MULTIPLE_INIT(pointer, &data);
		while (pointer != NULL) {
			DB_MULTIPLE_KEY_NEXT(pointer, &data, kp, klen, dp, len);
			if (kp != NULL)
				++total;
		}
	}
	TIMER_STOP;
	*totalp = total;

	dbp->dbenv->memp_stat(dbp->dbenv, &gsp, NULL, 0);
	DB_BENCH_ASSERT(gsp->st_cache_miss == 0);

#if 0
	fp = fopen("before", "w");
	dbp->set_msgfile(dbp, fp);
	DB_BENCH_ASSERT (dbp->stat_print(dbp, DB_STAT_ALL) == 0);
#endif

	DB_BENCH_ASSERT(dbp->close(dbp, 0) == 0);
}

void
op_tds(u_int ops, int update, u_int32_t flags)
{
	DB *dbp;
	DBT key, data;
	DB_ENV *dbenv;
	DB_TXN *txn;
	char *keybuf, *databuf;
	DB_MPOOL_STAT  *gsp;

	DB_BENCH_ASSERT((keybuf = malloc(keysize)) != NULL);
	DB_BENCH_ASSERT((databuf = malloc(datasize)) != NULL);

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));
	key.data = keybuf;
	key.size = keysize;
	memset(keybuf, 'a', keysize);

	data.data = databuf;
	data.size = datasize;
	memset(databuf, 'b', datasize);

	DB_BENCH_ASSERT(db_env_create(&dbenv, 0) == 0);

	dbenv->set_errfile(dbenv, stderr);

#ifdef DB_AUTO_COMMIT
	DB_BENCH_ASSERT(dbenv->set_flags(dbenv, DB_AUTO_COMMIT, 1) == 0);
#endif
	DB_BENCH_ASSERT(dbenv->set_flags(dbenv, flags, 1) == 0);
#ifdef DB_LOG_INMEMORY
	if (!(flags & DB_LOG_INMEMORY))
#endif
		DB_BENCH_ASSERT(dbenv->set_lg_max(dbenv, logbufsize * 10) == 0);
	DB_BENCH_ASSERT(dbenv->set_lg_bsize(dbenv, logbufsize) == 0);
	DB_BENCH_ASSERT(dbenv->open(dbenv, "TESTDIR",
	    DB_CREATE | DB_PRIVATE | DB_INIT_LOCK |
	    DB_INIT_LOG | DB_INIT_MPOOL | DB_INIT_TXN, 0666) == 0);

	DB_BENCH_ASSERT(db_create(&dbp, dbenv, 0) == 0);
	DB_BENCH_ASSERT(dbp->set_pagesize(dbp, pagesize) == 0);
	DB_BENCH_ASSERT(
	    dbp->open(dbp, NULL, "a", NULL, DB_BTREE, DB_CREATE, 0666) == 0);

	if (update) {
		dbenv->memp_stat(dbenv, &gsp, NULL, DB_STAT_CLEAR);

		TIMER_START;
		for (; ops > 0; --ops)
			DB_BENCH_ASSERT(
			    dbp->put(dbp, NULL, &key, &data, 0) == 0);
		TIMER_STOP;

		dbenv->memp_stat(dbenv, &gsp, NULL, 0);
		DB_BENCH_ASSERT(gsp->st_page_out == 0);
	} else {
		DB_BENCH_ASSERT(dbp->put(dbp, NULL, &key, &data, 0) == 0);
		dbenv->memp_stat(dbenv, &gsp, NULL, DB_STAT_CLEAR);

		TIMER_START;
		for (; ops > 0; --ops) {
			DB_BENCH_ASSERT(
			    dbenv->txn_begin(dbenv, NULL, &txn, 0) == 0);
			DB_BENCH_ASSERT(
			    dbp->get(dbp, NULL, &key, &data, 0) == 0);
			DB_BENCH_ASSERT(txn->commit(txn, 0) == 0);
		}
		TIMER_STOP;

		dbenv->memp_stat(dbenv, &gsp, NULL, 0);
		DB_BENCH_ASSERT(gsp->st_cache_miss == 0);
	}

	DB_BENCH_ASSERT(dbp->close(dbp, 0) == 0);
	DB_BENCH_ASSERT(dbenv->close(dbenv, 0) == 0);
}

#define	DEFAULT_OPS	1000000

int
main(int argc, char *argv[])
{
	extern char *optarg;
	extern int optind;
	u_int ops, total;
	int ch;

	cleanup_test_dir();

	if ((progname = strrchr(argv[0], '/')) == NULL)
		progname = argv[0];
	else
		++progname;

	ops = 0;
	while ((ch = getopt(argc, argv, "d:k:o:p:")) != EOF)
		switch (ch) {
		case 'd':
			datasize = (u_int)atoi(optarg);
			break;
		case 'k':
			keysize = (u_int)atoi(optarg);
			break;
		case 'o':
			ops = (u_int)atoi(optarg);
			break;
		case 'p':
			pagesize = (u_int32_t)atoi(optarg);
			break;
		case '?':
		default:
			return (usage());
		}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		return (usage());

	numitems = (cachesize / (keysize + datasize - 1)) / 2;

	if (strcasecmp(argv[0], "read") == 0) {
		if (ops == 0)
			ops = DEFAULT_OPS;
		op_ds(ops, 0);
		printf(
		    "# %u in-memory Btree database reads of %u/%u byte key/data pairs\n",
		    ops, keysize, datasize);
	} else if (strcasecmp(argv[0], "bulk") == 0) {
		if (keysize < 8) {
			fprintf(stderr,
		    "%s: bulk read requires a key size >= 10\n", progname);
			return (EXIT_FAILURE);
		}
		/*
		 * The ops value is the number of bulk operations, not key get
		 * operations.  Reduce the value so the test doesn't take so
		 * long, and use the returned number of retrievals as the ops
		 * value for timing purposes.
		 */
		if (ops == 0)
			ops = 100000;
		op_ds_bulk(ops, &total);
		ops = total;
		printf(
		    "# %u bulk in-memory Btree database reads of %u/%u byte key/data pairs\n",
		    ops, keysize, datasize);
	} else if (strcasecmp(argv[0], "write") == 0) {
		if (ops == 0)
			ops = DEFAULT_OPS;
		op_ds(ops, 1);
		printf(
		    "# %u in-memory Btree database writes of %u/%u byte key/data pairs\n",
		    ops, keysize, datasize);
	} else if (strcasecmp(argv[0], "txn-read") == 0) {
		if (ops == 0)
			ops = DEFAULT_OPS;
		op_tds(ops, 0, 0);
		printf(
		    "# %u transactional in-memory Btree database reads of %u/%u byte key/data pairs\n",
		    ops, keysize, datasize);
	} else if (strcasecmp(argv[0], "txn-write") == 0) {
		if (ops == 0)
			ops = DEFAULT_OPS;
#ifdef DB_LOG_INMEMORY
		op_tds(ops, 1, DB_LOG_INMEMORY);
		printf(
		    "# %u transactional in-memory logging Btree database writes of %u/%u byte key/data pairs\n",
		    ops, keysize, datasize);
#else
		return (EXIT_SUCCESS);
#endif
	} else if (strcasecmp(argv[0], "txn-nosync") == 0) {
		if (ops == 0)
			ops = DEFAULT_OPS;
		op_tds(ops, 1, DB_TXN_NOSYNC);
		printf(
		    "# %u transactional nosync logging Btree database writes of %u/%u byte key/data pairs\n",
		    ops, keysize, datasize);
	} else if (strcasecmp(argv[0], "txn-write-nosync") == 0) {
		if (ops == 0)
			ops = DEFAULT_OPS;
#ifdef DB_TXN_WRITE_NOSYNC
		op_tds(ops, 1, DB_TXN_WRITE_NOSYNC);
		printf(
		    "# %u transactional OS-write/nosync logging Btree database writes of %u/%u byte key/data pairs\n",
		    ops, keysize, datasize);
#else
		return (EXIT_SUCCESS);
#endif
	} else if (strcasecmp(argv[0], "txn-sync") == 0) {
		/*
		 * Flushing to disk takes a long time, reduce the number of
		 * default ops.
		 */
		if (ops == 0)
			ops = 100000;
		op_tds(ops, 1, 0);
		printf(
		    "# %u transactional logging Btree database writes of %u/%u byte key/data pairs\n",
		    ops, keysize, datasize);
	} else {
		fprintf(stderr, "%s: unknown keyword %s\n", progname, argv[0]);
		return (EXIT_FAILURE);
	}

	TIMER_DISPLAY(ops);
	return (EXIT_SUCCESS);
}

int
usage()
{
	fprintf(stderr, "usage: %s %s%s%s",
	    progname,
	    "[-d datasize] [-k keysize] [-o ops] [-p pagesize]\n\t",
	    "[read | bulk | write | txn-read |\n\t",
	    "txn-write | txn-nosync | txn-write-nosync | txn-sync]\n");
	return (EXIT_FAILURE);
}
#else
int
main()
{
	return (0);
}
#endif
