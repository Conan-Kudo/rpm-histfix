#include "system.h"

#include <rpmio_internal.h>
#define	_RPMGI_INTERNAL
#include <rpmgi.h>
#include <rpmcli.h>

#include <rpmmacro.h>
#include <rpmmessages.h>
#include <popt.h>

#include "debug.h"

static int gitag = RPMGI_FTSWALK;
static int ftsOpts = 0;

static struct poptOption optionsTable[] = {
 { "rpmgidebug", 'd', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmgi_debug, -1,
	N_("debug generalized iterator"), NULL},

 { "rpmdb", '\0', POPT_ARG_VAL, &gitag, RPMGI_RPMDB,
	N_("iterate rpmdb"), NULL },
 { "hdlist", '\0', POPT_ARG_VAL, &gitag, RPMGI_HDLIST,
	N_("iterate hdlist"), NULL },
 { "arglist", '\0', POPT_ARG_VAL, &gitag, RPMGI_ARGLIST,
	N_("iterate arglist"), NULL },
 { "ftswalk", '\0', POPT_ARG_VAL, &gitag, RPMGI_FTSWALK,
	N_("iterate fts(3) walk"), NULL },

 { "comfollow", '\0', POPT_BIT_SET,	&ftsOpts, FTS_COMFOLLOW,
	N_("follow command line symlinks"), NULL },
 { "logical", '\0', POPT_BIT_SET,	&ftsOpts, FTS_LOGICAL,
	N_("logical walk"), NULL },
 { "nochdir", '\0', POPT_BIT_SET,	&ftsOpts, FTS_NOCHDIR,
	N_("don't change directories"), NULL },
 { "nostat", '\0', POPT_BIT_SET,	&ftsOpts, FTS_NOSTAT,
	N_("don't get stat info"), NULL },
 { "physical", '\0', POPT_BIT_SET,	&ftsOpts, FTS_PHYSICAL,
	N_("physical walk"), NULL },
 { "seedot", '\0', POPT_BIT_SET,	&ftsOpts, FTS_SEEDOT,
	N_("return dot and dot-dot"), NULL },
 { "xdev", '\0', POPT_BIT_SET,		&ftsOpts, FTS_XDEV,
	N_("don't cross devices"), NULL },
 { "whiteout", '\0', POPT_BIT_SET,	&ftsOpts, FTS_WHITEOUT,
	N_("return whiteout information"), NULL },

#ifdef DYING
 { "ftpdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_ftp_debug, -1,
	N_("debug protocol data stream"), NULL},
 { "rpmiodebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmio_debug, -1,
	N_("debug rpmio I/O"), NULL},
 { "urldebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_url_debug, -1,
	N_("debug URL cache handling"), NULL},
 { "verbose", 'v', 0, 0, 'v',				NULL, NULL },
#endif

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmcliAllPoptTable, 0,
        N_("Common options for all rpm modes and executables:"),
        NULL },

  POPT_AUTOALIAS
  POPT_AUTOHELP
  POPT_TABLEEND
};

int
main(int argc, char *const argv[])
{
    poptContext optCon;
    rpmts ts = NULL;
    rpmgi gi = NULL;
    const char ** av;
    const char * arg;
    int ac;
    int rc = 0;

    optCon = rpmcliInit(argc, argv, optionsTable);
    if (optCon == NULL)
        exit(EXIT_FAILURE);

    if (ftsOpts == 0)
	ftsOpts = (FTS_COMFOLLOW | FTS_LOGICAL | FTS_NOSTAT);

    ts = rpmtsCreate();
    av = poptGetArgs(optCon);
    gi = rpmgiNew(ts, gitag, av, ftsOpts);

    ac = 0;
    while ((arg = rpmgiNext(gi)) != NULL) {
	fprintf(stderr, "%5d %s\n", ac, arg);
	arg = _free(arg);
	ac++;
    }

    gi = rpmgiFree(gi);
    ts = rpmtsFree(ts);
    optCon = rpmcliFini(optCon);

    return rc;
}
