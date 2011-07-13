/** \ingroup rpmcli
 * \file lib/poptQV.c
 *  Popt tables for query/verify modes.
 */

#include "system.h"

#include <rpm/rpmcli.h>
#include "lib/rpmgi.h"	/* XXX for giFlags */

#include "debug.h"

struct rpmQVKArguments_s rpmQVKArgs;

#define POPT_QUERYFORMAT	-1000
#define POPT_WHATREQUIRES	-1001
#define POPT_WHATPROVIDES	-1002
#define POPT_QUERYBYNUMBER	-1003
#define POPT_TRIGGEREDBY	-1004
#define POPT_DUMP		-1005
#define POPT_QUERYBYPKGID	-1007
#define POPT_QUERYBYHDRID	-1008
#define POPT_QUERYBYTID		-1010

/* ========== Query/Verify/Signature source args */
static void rpmQVSourceArgCallback( poptContext con,
		enum poptCallbackReason reason,
		const struct poptOption * opt, const char * arg, 
		const void * data)
{
    QVA_t qva = &rpmQVKArgs;
    rpmQVSources sources = qva->qva_source;;

    switch (opt->val) {
    case 'q':	/* from --query, -q */
    case 'Q':	/* from --querytags (handled by poptALL) */
    case 'V':	/* from --verify, -V */
	if (qva->qva_mode == '\0' || strchr("qQ ", qva->qva_mode)) {
	    qva->qva_mode = opt->val;
	}
	break;
    case 'a': qva->qva_source |= RPMQV_ALL; break;
    case 'f': qva->qva_source |= RPMQV_PATH; break;
    case 'g': qva->qva_source |= RPMQV_GROUP; break;
    case 'p': qva->qva_source |= RPMQV_RPM; break;
    case POPT_WHATPROVIDES: qva->qva_source |= RPMQV_WHATPROVIDES; break;
    case POPT_WHATREQUIRES: qva->qva_source |= RPMQV_WHATREQUIRES; break;
    case POPT_TRIGGEREDBY: qva->qva_source |= RPMQV_TRIGGEREDBY; break;
    case POPT_QUERYBYPKGID: qva->qva_source |= RPMQV_PKGID; break;
    case POPT_QUERYBYHDRID: qva->qva_source |= RPMQV_HDRID; break;
    case POPT_QUERYBYTID: qva->qva_source |= RPMQV_TID; break;
    case POPT_QUERYBYNUMBER: qva->qva_source |= RPMQV_DBOFFSET; break;
    }

    if (sources != qva->qva_source)
	qva->qva_sourceCount++;
}

/**
 * Common query/verify mode options.
 */
struct poptOption rpmQVSourcePoptTable[] = {
/* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA, 
	rpmQVSourceArgCallback, 0, NULL, NULL },
 { "all", 'a', 0, 0, 'a',
	N_("query/verify all packages"), NULL },
 { "checksig", 'K', POPT_ARGFLAG_DOC_HIDDEN, NULL, 'K',
	N_("rpm checksig mode"), NULL },
 { "file", 'f', 0, 0, 'f',
	N_("query/verify package(s) owning file"), "FILE" },
 { "group", 'g', 0, 0, 'g',
	N_("query/verify package(s) in group"), "GROUP" },
 { "package", 'p', 0, 0, 'p',
	N_("query/verify a package file"), NULL },

 { "pkgid", '\0', 0, 0, POPT_QUERYBYPKGID,
	N_("query/verify package(s) with package identifier"), "MD5" },
 { "hdrid", '\0', 0, 0, POPT_QUERYBYHDRID,
	N_("query/verify package(s) with header identifier"), "SHA1" },

 { "query", 'q', POPT_ARGFLAG_DOC_HIDDEN, NULL, 'q',
	N_("rpm query mode"), NULL },
 { "querybynumber", '\0', POPT_ARGFLAG_DOC_HIDDEN, 0, POPT_QUERYBYNUMBER,
	N_("query/verify a header instance"), "HDRNUM" },
 { "tid", '\0', POPT_ARGFLAG_DOC_HIDDEN, 0, POPT_QUERYBYTID,
	N_("query/verify package(s) from install transaction"), "TID" },
 { "triggeredby", '\0', 0, 0, POPT_TRIGGEREDBY, 
	N_("query the package(s) triggered by the package"), "PACKAGE" },
 { "verify", 'V', POPT_ARGFLAG_DOC_HIDDEN, NULL, 'V',
	N_("rpm verify mode"), NULL },
 { "whatrequires", '\0', 0, 0, POPT_WHATREQUIRES, 
	N_("query/verify the package(s) which require a dependency"), "CAPABILITY" },
 { "whatprovides", '\0', 0, 0, POPT_WHATPROVIDES, 
	N_("query/verify the package(s) which provide a dependency"), "CAPABILITY" },

 { "noglob", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN, &giFlags, RPMGI_NOGLOB,
	N_("do not glob arguments"), NULL},
 { "nomanifest", '\0', POPT_BIT_SET, &giFlags, RPMGI_NOMANIFEST,
	N_("do not process non-package files as manifests"), NULL},

   POPT_TABLEEND
};

/* ========== Query specific popt args */

static void queryArgCallback(poptContext con,
		enum poptCallbackReason reason,
		const struct poptOption * opt, const char * arg, 
		const void * data)
{
    QVA_t qva = &rpmQVKArgs;

    switch (opt->val) {
    case 'c': qva->qva_flags |= QUERY_FOR_CONFIG | QUERY_FOR_LIST; break;
    case 'd': qva->qva_flags |= QUERY_FOR_DOCS | QUERY_FOR_LIST; break;
    case 'l': qva->qva_flags |= QUERY_FOR_LIST; break;
    case 's': qva->qva_flags |= QUERY_FOR_STATE | QUERY_FOR_LIST;
	break;
    case POPT_DUMP: qva->qva_flags |= QUERY_FOR_DUMPFILES | QUERY_FOR_LIST;
	break;

    case POPT_QUERYFORMAT:
	rstrcat(&qva->qva_queryFormat, arg);
	break;

    case 'i':
	if (qva->qva_mode == 'q') {
	    const char * infoCommand[] = { "--info", NULL };
	    (void) poptStuffArgs(con, infoCommand);
	}
	break;

    case RPMCLI_POPT_NODEPS:
	qva->qva_flags |= VERIFY_DEPS;
	break;

    case RPMCLI_POPT_NOFILEDIGEST:
	qva->qva_flags |= VERIFY_FILEDIGEST;
	break;

    case RPMCLI_POPT_NOCONTEXTS:
	qva->qva_flags |= VERIFY_CONTEXTS;
	break;

#ifdef	NOTYET
    case RPMCLI_POPT_FORCE:
	ia->probFilter |=
		( RPMPROB_FILTER_REPLACEPKG
		| RPMPROB_FILTER_REPLACEOLDFILES
		| RPMPROB_FILTER_REPLACENEWFILES
		| RPMPROB_FILTER_OLDPACKAGE );
	break;
#endif

    case RPMCLI_POPT_NOSCRIPTS:
	qva->qva_flags |= VERIFY_SCRIPT;
	break;

    }
}

/**
 * Query mode options.
 */
struct poptOption rpmQueryPoptTable[] = {
/* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE, 
	queryArgCallback, 0, NULL, NULL },
 { "configfiles", 'c', 0, 0, 'c',
	N_("list all configuration files"), NULL },
 { "docfiles", 'd', 0, 0, 'd',
	N_("list all documentation files"), NULL },
 { "dump", '\0', 0, 0, POPT_DUMP,
	N_("dump basic file information"), NULL },
 { NULL, 'i', POPT_ARGFLAG_DOC_HIDDEN, 0, 'i',
	NULL, NULL },
 { "list", 'l', 0, 0, 'l',
	N_("list files in package"), NULL },

 /* Duplicate file attr flags from packages into command line options. */
 { "noghost", '\0', POPT_BIT_CLR|POPT_ARGFLAG_DOC_HIDDEN,
	&rpmQVKArgs.qva_fflags, RPMFILE_GHOST,
        N_("skip %%ghost files"), NULL },

 { "qf", '\0', POPT_ARG_STRING | POPT_ARGFLAG_DOC_HIDDEN, 0, 
	POPT_QUERYFORMAT, NULL, NULL },
 { "queryformat", '\0', POPT_ARG_STRING, 0, POPT_QUERYFORMAT,
	N_("use the following query format"), "QUERYFORMAT" },
 { "state", 's', 0, 0, 's',
	N_("display the states of the listed files"), NULL },
   POPT_TABLEEND
};

/**
 * Verify mode options.
 */
struct poptOption rpmVerifyPoptTable[] = {
/* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE, 
	queryArgCallback, 0, NULL, NULL },

 { "nofiledigest", '\0', 0, NULL, RPMCLI_POPT_NOFILEDIGEST,
	N_("don't verify digest of files"), NULL },
 { "nomd5", '\0', POPT_ARGFLAG_DOC_HIDDEN, NULL, RPMCLI_POPT_NOFILEDIGEST,
	N_("don't verify digest of files"), NULL },
 { "nosize", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,
	&rpmQVKArgs.qva_flags, VERIFY_SIZE,
        N_("don't verify size of files"), NULL },
 { "nolinkto", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,
	&rpmQVKArgs.qva_flags, VERIFY_LINKTO,
        N_("don't verify symlink path of files"), NULL },
 { "nouser", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,
	&rpmQVKArgs.qva_flags, VERIFY_USER,
        N_("don't verify owner of files"), NULL },
 { "nogroup", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,
	&rpmQVKArgs.qva_flags, VERIFY_GROUP,
        N_("don't verify group of files"), NULL },
 { "nomtime", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,
	&rpmQVKArgs.qva_flags, VERIFY_MTIME,
        N_("don't verify modification time of files"), NULL },
 { "nomode", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,
	&rpmQVKArgs.qva_flags, VERIFY_MODE,
        N_("don't verify mode of files"), NULL },
 { "nordev", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,
	&rpmQVKArgs.qva_flags, VERIFY_RDEV,
        N_("don't verify mode of files"), NULL },
 { "nocaps", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,
	&rpmQVKArgs.qva_flags, VERIFY_CAPS,
	N_("don't verify capabilities of files"), NULL },

 { "nocontexts", '\0', POPT_ARGFLAG_DOC_HIDDEN, NULL, RPMCLI_POPT_NOCONTEXTS,
	N_("don't verify file security contexts"), NULL },
 { "nofiles", '\0', POPT_BIT_SET, &rpmQVKArgs.qva_flags, VERIFY_FILES,
	N_("don't verify files in package"), NULL},
 { "nodeps", '\0', 0, NULL, RPMCLI_POPT_NODEPS,
	N_("don't verify package dependencies"), NULL },

 { "noscript", '\0', 0, NULL, RPMCLI_POPT_NOSCRIPTS,
        N_("don't execute verify script(s)"), NULL },
 /* XXX legacy had a trailing s on --noscript */
 { "noscripts", '\0', POPT_ARGFLAG_DOC_HIDDEN, NULL, RPMCLI_POPT_NOSCRIPTS,
        N_("don't execute verify script(s)"), NULL },

    POPT_TABLEEND
};
