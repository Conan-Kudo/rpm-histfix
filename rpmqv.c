#include "system.h"
const char *__progname;

#include <rpm/rpmcli.h>
#include <rpm/rpmlib.h>			/* RPMSIGTAG, rpmReadPackageFile .. */
#include <rpm/rpmbuild.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmdb.h>
#include <rpm/rpmps.h>
#include <rpm/rpmts.h>

#if defined(IAM_RPMK)
#include "lib/signature.h"
#endif
#include "cliutils.h"

#include "debug.h"

enum modes {

    MODE_QUERY		= (1 <<  0),
    MODE_VERIFY		= (1 <<  3),
#define	MODES_QV (MODE_QUERY | MODE_VERIFY)

    MODE_INSTALL	= (1 <<  1),
    MODE_ERASE		= (1 <<  2),
#define	MODES_IE (MODE_INSTALL | MODE_ERASE)

    MODE_CHECKSIG	= (1 <<  6),
    MODE_RESIGN		= (1 <<  7),
    MODE_KEYRING	= (1 <<  8),
#define	MODES_K	 (MODE_CHECKSIG | MODE_RESIGN | MODE_KEYRING)

    MODE_INITDB		= (1 << 10),
    MODE_REBUILDDB	= (1 << 12),
    MODE_VERIFYDB	= (1 << 13),
#define	MODES_DB (MODE_INITDB | MODE_REBUILDDB | MODE_VERIFYDB)


    MODE_UNKNOWN	= 0
};

#define	MODES_FOR_DBPATH	(MODES_IE | MODES_QV | MODES_DB)
#define	MODES_FOR_NODEPS	(MODES_IE | MODE_VERIFY)
#define	MODES_FOR_TEST		(MODES_IE)
#define	MODES_FOR_ROOT		(MODES_IE | MODES_QV | MODES_DB | MODES_K)

static int quiet;

/* the structure describing the options we take and the defaults */
static struct poptOption optionsTable[] = {

#ifdef	IAM_RPMQV
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmQVSourcePoptTable, 0,
        N_("Query/Verify package selection options:"),
        NULL },
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmQueryPoptTable, 0,
	N_("Query options (with -q or --query):"),
	NULL },
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmVerifyPoptTable, 0,
	N_("Verify options (with -V or --verify):"),
	NULL },
#endif	/* IAM_RPMQV */

#ifdef	IAM_RPMK
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmSignPoptTable, 0,
	N_("Signature options:"),
	NULL },
#endif	/* IAM_RPMK */

#ifdef	IAM_RPMDB
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmDatabasePoptTable, 0,
	N_("Database options:"),
	NULL },
#endif	/* IAM_RPMDB */

#ifdef	IAM_RPMEIU
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmInstallPoptTable, 0,
	N_("Install/Upgrade/Erase options:"),
	NULL },
#endif	/* IAM_RPMEIU */

 { "quiet", '\0', POPT_ARGFLAG_DOC_HIDDEN, &quiet, 0, NULL, NULL},

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmcliAllPoptTable, 0,
	N_("Common options for all rpm modes and executables:"),
	NULL },

   POPT_AUTOALIAS
   POPT_AUTOHELP
   POPT_TABLEEND
};

int main(int argc, char *argv[])
{
    rpmts ts = NULL;
    enum modes bigMode = MODE_UNKNOWN;

#if defined(IAM_RPMQV)
    QVA_t qva = &rpmQVKArgs;
#endif

#ifdef	IAM_RPMEIU
   struct rpmInstallArguments_s * ia = &rpmIArgs;
#endif

#if defined(IAM_RPMDB)
   struct rpmDatabaseArguments_s * da = &rpmDBArgs;
#endif

#if defined(IAM_RPMK)
   QVA_t ka = &rpmQVKArgs;
#endif

#if defined(IAM_RPMK)
    char * passPhrase = "";
#endif

    poptContext optCon;
    int ec = 0;
#ifdef	IAM_RPMEIU
    int i;
#endif

    optCon = rpmcliInit(argc, argv, optionsTable);

    /* Set the major mode based on argv[0] */
#ifdef	IAM_RPMQV
    if (rstreq(__progname, "rpmquery"))	bigMode = MODE_QUERY;
    if (rstreq(__progname, "rpmverify")) bigMode = MODE_VERIFY;
#endif

#if defined(IAM_RPMQV)
    /* Jumpstart option from argv[0] if necessary. */
    switch (bigMode) {
    case MODE_QUERY:	qva->qva_mode = 'q';	break;
    case MODE_VERIFY:	qva->qva_mode = 'V';	break;
    case MODE_CHECKSIG:	qva->qva_mode = 'K';	break;
    case MODE_RESIGN:	qva->qva_mode = 'R';	break;
    case MODE_INSTALL:
    case MODE_ERASE:
    case MODE_INITDB:
    case MODE_REBUILDDB:
    case MODE_VERIFYDB:
    case MODE_UNKNOWN:
    default:
	break;
    }
#endif

#ifdef	IAM_RPMDB
  if (bigMode == MODE_UNKNOWN || (bigMode & MODES_DB)) {
    if (da->init) {
	if (bigMode != MODE_UNKNOWN) 
	    argerror(_("only one major mode may be specified"));
	else
	    bigMode = MODE_INITDB;
    } else
    if (da->rebuild) {
	if (bigMode != MODE_UNKNOWN) 
	    argerror(_("only one major mode may be specified"));
	else
	    bigMode = MODE_REBUILDDB;
    } else
    if (da->verify) {
	if (bigMode != MODE_UNKNOWN) 
	    argerror(_("only one major mode may be specified"));
	else
	    bigMode = MODE_VERIFYDB;
    }
  }
#endif	/* IAM_RPMDB */

#ifdef	IAM_RPMQV
  if (bigMode == MODE_UNKNOWN || (bigMode & MODES_QV)) {
    switch (qva->qva_mode) {
    case 'q':	bigMode = MODE_QUERY;		break;
    case 'V':	bigMode = MODE_VERIFY;		break;
    }

    if (qva->qva_sourceCount) {
	if (qva->qva_sourceCount > 2)
	    argerror(_("one type of query/verify may be performed at a "
			"time"));
    }
    if (qva->qva_flags && (bigMode & ~MODES_QV)) 
	argerror(_("unexpected query flags"));

    if (qva->qva_queryFormat && (bigMode & ~MODES_QV)) 
	argerror(_("unexpected query format"));

    if (qva->qva_source != RPMQV_PACKAGE && (bigMode & ~MODES_QV)) 
	argerror(_("unexpected query source"));
  }
#endif	/* IAM_RPMQV */

#ifdef	IAM_RPMEIU
  if (bigMode == MODE_UNKNOWN || (bigMode & MODES_IE))
    {	int iflags = (ia->installInterfaceFlags &
		(INSTALL_UPGRADE|INSTALL_FRESHEN|INSTALL_INSTALL));
	int eflags = (ia->installInterfaceFlags & INSTALL_ERASE);

	if (iflags & eflags)
	    argerror(_("only one major mode may be specified"));
	else if (iflags)
	    bigMode = MODE_INSTALL;
	else if (eflags)
	    bigMode = MODE_ERASE;
    }
#endif	/* IAM_RPMEIU */

#ifdef	IAM_RPMK
  if (bigMode == MODE_UNKNOWN || (bigMode & MODES_K)) {
	switch (ka->qva_mode) {
	case RPMSIGN_IMPORT_PUBKEY:
	    bigMode = MODE_KEYRING;
	    break;
	case RPMSIGN_CHK_SIGNATURE:
	    bigMode = MODE_CHECKSIG;
	    break;
	case RPMSIGN_ADD_SIGNATURE:
	case RPMSIGN_NEW_SIGNATURE:
	    ka->sign = 1;
	    /* fallthrough */
	case RPMSIGN_DEL_SIGNATURE:
	    bigMode = MODE_RESIGN;
	    break;
	}
  }
#endif	/* IAM_RPMK */

#if defined(IAM_RPMEIU)
    if (!( bigMode == MODE_INSTALL ) &&
(ia->probFilter & (RPMPROB_FILTER_REPLACEPKG | RPMPROB_FILTER_OLDPACKAGE)))
	argerror(_("only installation, upgrading, rmsource and rmspec may be forced"));
    if (bigMode != MODE_INSTALL && (ia->probFilter & RPMPROB_FILTER_FORCERELOCATE))
	argerror(_("files may only be relocated during package installation"));

    if (ia->relocations && ia->prefix)
	argerror(_("cannot use --prefix with --relocate or --excludepath"));

    if (bigMode != MODE_INSTALL && ia->relocations)
	argerror(_("--relocate and --excludepath may only be used when installing new packages"));

    if (bigMode != MODE_INSTALL && ia->prefix)
	argerror(_("--prefix may only be used when installing new packages"));

    if (ia->prefix && ia->prefix[0] != '/') 
	argerror(_("arguments to --prefix must begin with a /"));

    if (bigMode != MODE_INSTALL && (ia->installInterfaceFlags & INSTALL_HASH))
	argerror(_("--hash (-h) may only be specified during package "
			"installation"));

    if (bigMode != MODE_INSTALL && (ia->installInterfaceFlags & INSTALL_PERCENT))
	argerror(_("--percent may only be specified during package "
			"installation"));

    if (bigMode != MODE_INSTALL && (ia->probFilter & RPMPROB_FILTER_REPLACEPKG))
	argerror(_("--replacepkgs may only be specified during package "
			"installation"));

    if (bigMode != MODE_INSTALL && (ia->transFlags & RPMTRANS_FLAG_NODOCS))
	argerror(_("--excludedocs may only be specified during package "
		   "installation"));

    if (bigMode != MODE_INSTALL && ia->incldocs)
	argerror(_("--includedocs may only be specified during package "
		   "installation"));

    if (ia->incldocs && (ia->transFlags & RPMTRANS_FLAG_NODOCS))
	argerror(_("only one of --excludedocs and --includedocs may be "
		 "specified"));
  
    if (bigMode != MODE_INSTALL && (ia->probFilter & RPMPROB_FILTER_IGNOREARCH))
	argerror(_("--ignorearch may only be specified during package "
		   "installation"));

    if (bigMode != MODE_INSTALL && (ia->probFilter & RPMPROB_FILTER_IGNOREOS))
	argerror(_("--ignoreos may only be specified during package "
		   "installation"));

    if (bigMode != MODE_INSTALL && bigMode != MODE_ERASE &&
	(ia->probFilter & (RPMPROB_FILTER_DISKSPACE|RPMPROB_FILTER_DISKNODES)))
	argerror(_("--ignoresize may only be specified during package "
		   "installation"));

    if ((ia->installInterfaceFlags & UNINSTALL_ALLMATCHES) && bigMode != MODE_ERASE)
	argerror(_("--allmatches may only be specified during package "
		   "erasure"));

    if ((ia->transFlags & RPMTRANS_FLAG_ALLFILES) && bigMode != MODE_INSTALL)
	argerror(_("--allfiles may only be specified during package "
		   "installation"));

    if ((ia->transFlags & RPMTRANS_FLAG_JUSTDB) &&
	bigMode != MODE_INSTALL && bigMode != MODE_ERASE)
	argerror(_("--justdb may only be specified during package "
		   "installation and erasure"));

    if (bigMode != MODE_INSTALL && bigMode != MODE_ERASE && bigMode != MODE_VERIFY &&
	(ia->transFlags & (RPMTRANS_FLAG_NOSCRIPTS | _noTransScripts | _noTransTriggers)))
	argerror(_("script disabling options may only be specified during "
		   "package installation and erasure"));

    if (bigMode != MODE_INSTALL && bigMode != MODE_ERASE && bigMode != MODE_VERIFY &&
	(ia->transFlags & (RPMTRANS_FLAG_NOTRIGGERS | _noTransTriggers)))
	argerror(_("trigger disabling options may only be specified during "
		   "package installation and erasure"));

    if (ia->noDeps & (bigMode & ~MODES_FOR_NODEPS))
	argerror(_("--nodeps may only be specified during package "
		   "building, rebuilding, recompilation, installation,"
		   "erasure, and verification"));

    if ((ia->transFlags & RPMTRANS_FLAG_TEST) && (bigMode & ~MODES_FOR_TEST))
	argerror(_("--test may only be specified during package installation, "
		 "erasure, and building"));
#endif	/* IAM_RPMEIU */

    if (rpmcliRootDir && rpmcliRootDir[1] && (bigMode & ~MODES_FOR_ROOT))
	argerror(_("--root (-r) may only be specified during "
		 "installation, erasure, querying, and "
		 "database rebuilds"));

    if (rpmcliRootDir && rpmcliRootDir[0] != '/') {
	argerror(_("arguments to --root (-r) must begin with a /"));
    }

    if (quiet)
	rpmSetVerbosity(RPMLOG_WARNING);

#if defined(IAM_RPMK)
    if (ka->sign) {
	if (bigMode == MODE_RESIGN) {
	    const char ** av;
	    struct stat sb;
	    int errors = 0;

	    if ((av = poptGetArgs(optCon)) == NULL) {
		fprintf(stderr, _("no files to sign\n"));
		errors++;
	    } else
	    while (*av) {
		if (stat(*av, &sb)) {
		    fprintf(stderr, _("cannot access file %s\n"), *av);
		    errors++;
		}
		av++;
	    }

	    if (errors) {
		ec = errors;
		goto exit;
	    }

            if (poptPeekArg(optCon)) {
		int sigTag = rpmLookupSignatureType(RPMLOOKUPSIG_QUERY);
		switch (sigTag) {
		  case 0:
		    break;
		  case RPMSIGTAG_PGP:
		  case RPMSIGTAG_GPG:
		  case RPMSIGTAG_DSA:
		  case RPMSIGTAG_RSA:
		    passPhrase = rpmGetPassPhrase(_("Enter pass phrase: "), sigTag);
		    if (passPhrase == NULL) {
			fprintf(stderr, _("Pass phrase check failed\n"));
			ec = EXIT_FAILURE;
			goto exit;
		    }
		    fprintf(stderr, _("Pass phrase is good.\n"));
		    passPhrase = xstrdup(passPhrase);
		    break;
		  default:
		    fprintf(stderr,
		            _("Invalid %%_signature spec in macro file.\n"));
		    ec = EXIT_FAILURE;
		    goto exit;
		    break;
		}
	    }
	} else {
	    argerror(_("--sign may only be used during package building"));
	}
    } else {
    	/* Make rpmLookupSignatureType() return 0 ("none") from now on */
        (void) rpmLookupSignatureType(RPMLOOKUPSIG_DISABLE);
    }
#endif	/* IAM_RPMK */

    if (rpmcliPipeOutput && initPipe())
	exit(EXIT_FAILURE);
	
    ts = rpmtsCreate();
    (void) rpmtsSetRootDir(ts, rpmcliRootDir);
    switch (bigMode) {
#ifdef	IAM_RPMDB
    case MODE_INITDB:
	ec = rpmtsInitDB(ts, 0644);
	break;

    case MODE_REBUILDDB:
    {   rpmVSFlags vsflags = rpmExpandNumeric("%{_vsflags_rebuilddb}");
	rpmVSFlags ovsflags = rpmtsSetVSFlags(ts, vsflags);
	ec = rpmtsRebuildDB(ts);
	vsflags = rpmtsSetVSFlags(ts, ovsflags);
    }	break;
    case MODE_VERIFYDB:
	ec = rpmtsVerifyDB(ts);
	break;
#endif	/* IAM_RPMDB */

#ifdef	IAM_RPMEIU
    case MODE_ERASE:
	if (ia->noDeps) ia->installInterfaceFlags |= UNINSTALL_NODEPS;

	if (!poptPeekArg(optCon)) {
	    argerror(_("no packages given for erase"));
	} else {
	    ec += rpmErase(ts, ia, (ARGV_const_t) poptGetArgs(optCon));
	}
	break;

    case MODE_INSTALL:

	/* RPMTRANS_FLAG_KEEPOBSOLETE */

	if (!ia->incldocs) {
	    if (ia->transFlags & RPMTRANS_FLAG_NODOCS) {
		;
	    } else if (rpmExpandNumeric("%{_excludedocs}"))
		ia->transFlags |= RPMTRANS_FLAG_NODOCS;
	}

	if (ia->noDeps) ia->installInterfaceFlags |= INSTALL_NODEPS;

	/* we've already ensured !(!ia->prefix && !ia->relocations) */
	if (ia->prefix) {
	    ia->relocations = xmalloc(2 * sizeof(*ia->relocations));
	    ia->relocations[0].oldPath = NULL;   /* special case magic */
	    ia->relocations[0].newPath = ia->prefix;
	    ia->relocations[1].oldPath = NULL;
	    ia->relocations[1].newPath = NULL;
	} else if (ia->relocations) {
	    ia->relocations = xrealloc(ia->relocations, 
			sizeof(*ia->relocations) * (ia->numRelocations + 1));
	    ia->relocations[ia->numRelocations].oldPath = NULL;
	    ia->relocations[ia->numRelocations].newPath = NULL;
	}

	if (!poptPeekArg(optCon)) {
	    argerror(_("no packages given for install"));
	} else {
	    /* FIX: ia->relocations[0].newPath undefined */
	    ec += rpmInstall(ts, ia, (ARGV_t) poptGetArgs(optCon));
	}
	break;

#endif	/* IAM_RPMEIU */

#ifdef	IAM_RPMQV
    case MODE_QUERY:
	if (!poptPeekArg(optCon) && !(qva->qva_source == RPMQV_ALL))
	    argerror(_("no arguments given for query"));

	qva->qva_specQuery = rpmspecQuery;
	ec = rpmcliQuery(ts, qva, (ARGV_const_t) poptGetArgs(optCon));
	qva->qva_specQuery = NULL;
	break;

    case MODE_VERIFY:
    {	rpmVerifyFlags verifyFlags = VERIFY_ALL;

	verifyFlags &= ~qva->qva_flags;
	qva->qva_flags = (rpmQueryFlags) verifyFlags;

	if (!poptPeekArg(optCon) && !(qva->qva_source == RPMQV_ALL))
	    argerror(_("no arguments given for verify"));
	ec = rpmcliVerify(ts, qva, (ARGV_const_t) poptGetArgs(optCon));
    }	break;
#endif	/* IAM_RPMQV */

#ifdef IAM_RPMK
    case MODE_KEYRING:
	if (!poptPeekArg(optCon))
	    argerror(_("no arguments given"));
	ec = rpmcliImportPubkeys(ts, (ARGV_const_t) poptGetArgs(optCon));
	break;
    case MODE_CHECKSIG:
    {	rpmVerifyFlags verifyFlags =
		(VERIFY_FILEDIGEST|VERIFY_DIGEST|VERIFY_SIGNATURE);

	verifyFlags &= ~rpmcliQueryFlags;
	ka->qva_flags = (rpmQueryFlags) verifyFlags;
	ec = rpmcliVerifySignatures(ts, ka, (ARGV_const_t) poptGetArgs(optCon));
	break;
    }  
    case MODE_RESIGN:
	if (!poptPeekArg(optCon))
	    argerror(_("no arguments given"));
	ec = rpmcliSign((ARGV_const_t) poptGetArgs(optCon),
			(qva->qva_mode == RPMSIGN_DEL_SIGNATURE), passPhrase);
    	break;
#endif	/* IAM_RPMK */
	
#if !defined(IAM_RPMQV)
    case MODE_QUERY:
    case MODE_VERIFY:
#endif
#if !defined(IAM_RPMK)
    case MODE_CHECKSIG:
    case MODE_RESIGN:
#endif
#if !defined(IAM_RPMDB)
    case MODE_INITDB:
    case MODE_REBUILDDB:
    case MODE_VERIFYDB:
#endif
#if !defined(IAM_RPMEIU)
    case MODE_INSTALL:
    case MODE_ERASE:
#endif
    case MODE_UNKNOWN:
	if (poptPeekArg(optCon) != NULL || argc <= 1 || rpmIsVerbose()) {
	    printUsage(optCon, stderr, 0);
	    ec = argc;
	}
	break;
    }

exit:
    ts = rpmtsFree(ts);
    finishPipe();

#ifdef	IAM_RPMQV
    qva->qva_queryFormat = _free(qva->qva_queryFormat);
#endif

#ifdef	IAM_RPMEIU
    if (ia->relocations != NULL)
    for (i = 0; i < ia->numRelocations; i++)
	ia->relocations[i].oldPath = _free(ia->relocations[i].oldPath);
    ia->relocations = _free(ia->relocations);
#endif

    rpmcliFini(optCon);

    return RETVAL(ec);
}
