#include "system.h"

#include <rpmio_internal.h>
#include <rpmgi.h>
#include <rpmcli.h>

#include <rpmmacro.h>
#include <rpmmessages.h>
#include <popt.h>

#include "debug.h"

static const char * gitagstr = "packages";
static const char * gikeystr = NULL;
static int ftsOpts = 0;

static const char * queryFormat = NULL;

/*@only@*/ /*@null@*/
static const char * rpmgiPathOrQF(const rpmgi gi)
	/*@*/
{
    const char * fmt = ((queryFormat != NULL)
	? queryFormat : "%{name}-%{version}-%{release}.%{arch}");
    const char * val = NULL;
    Header h = rpmgiHeader(gi);

    if (h != NULL)
	val = headerSprintf(h, fmt, rpmTagTable, rpmHeaderFormats, NULL);
    else {
	const char * fn = rpmgiHdrPath(gi);
	val = (fn != NULL ? xstrdup(fn) : NULL);
    }

    return val;
}

static struct poptOption optionsTable[] = {
 { "rpmgidebug", 'd', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmgi_debug, -1,
	N_("debug generalized iterator"), NULL},

 { "tag", '\0', POPT_ARG_STRING|POPT_ARGFLAG_SHOW_DEFAULT, &gitagstr, 0,
	N_("iterate tag index"), NULL },
 { "key", '\0', POPT_ARG_STRING|POPT_ARGFLAG_SHOW_DEFAULT, &gikeystr, 0,
	N_("tag value key"), NULL },

 { "qf", '\0', POPT_ARG_STRING, &queryFormat, 0,
        N_("use the following query format"), "QUERYFORMAT" },
 { "queryformat", '\0', POPT_ARG_STRING, &queryFormat, 0,
        N_("use the following query format"), "QUERYFORMAT" },

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
    rpmVSFlags vsflags;
    rpmgi gi = NULL;
    int gitag = RPMDBI_PACKAGES;
    const char ** av;
    int ac;
    int rc = 0;

    optCon = rpmcliInit(argc, argv, optionsTable);
    if (optCon == NULL)
        exit(EXIT_FAILURE);

    if (ftsOpts == 0)
	ftsOpts = (FTS_COMFOLLOW | FTS_LOGICAL | FTS_NOSTAT);

    if (gitagstr != NULL) {
	gitag = tagValue(gitagstr);
	if (gitag < 0) {
	    fprintf(stderr, _("unknown --tag argument: %s\n"), gitagstr);
	    exit(EXIT_FAILURE);
	}
    }

    /* XXX ftswalk segfault with no args. */

    ts = rpmtsCreate();
    vsflags = rpmExpandNumeric("%{?_vsflags_query}");
    if (rpmcliQueryFlags & VERIFY_DIGEST)
	vsflags |= _RPMVSF_NODIGESTS;
    if (rpmcliQueryFlags & VERIFY_SIGNATURE)
	vsflags |= _RPMVSF_NOSIGNATURES;
    if (rpmcliQueryFlags & VERIFY_HDRCHK)
	vsflags |= RPMVSF_NOHDRCHK;
    (void) rpmtsSetVSFlags(ts, vsflags);

    {   int_32 tid = (int_32) time(NULL);
	(void) rpmtsSetTid(ts, tid);
    }

    gi = rpmgiNew(ts, gitag, gikeystr, 0);

    av = poptGetArgs(optCon);
    (void) rpmgiSetArgs(gi, av, ftsOpts);

    ac = 0;
    while (rpmgiNext(gi) == RPMRC_OK) {
	const char * arg = rpmgiPathOrQF(gi);

	fprintf(stderr, "%5d %s\n", ac, arg);
	arg = _free(arg);
	ac++;
    }

    gi = rpmgiFree(gi);
    ts = rpmtsFree(ts);
    optCon = rpmcliFini(optCon);

    return rc;
}
