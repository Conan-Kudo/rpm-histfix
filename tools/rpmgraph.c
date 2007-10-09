#include "system.h"
const char *__progname;

#include <rpmcli.h>

#include "rpmdb.h"
#include "rpmps.h"

#include "rpmte.h"

#define _RPMTS_INTERNAL         /* ts->goal, ts->dbmode, ts->suggests */
#include "rpmts.h"

#include "manifest.h"
#include "misc.h"		/* rpmGlob */
#include "debug.h"

static int noDeps = 1;

static rpmVSFlags vsflags = 0;

static inline const char * identifyDepend(int_32 f)
{
    if (isLegacyPreReq(f))
	return "PreReq:";
    f = _notpre(f);
    if (f & RPMSENSE_SCRIPT_PRE)
	return "Requires(pre):";
    if (f & RPMSENSE_SCRIPT_POST)
	return "Requires(post):";
    if (f & RPMSENSE_SCRIPT_PREUN)
	return "Requires(preun):";
    if (f & RPMSENSE_SCRIPT_POSTUN)
	return "Requires(postun):";
    if (f & RPMSENSE_SCRIPT_VERIFY)
	return "Requires(verify):";
    if (f & RPMSENSE_FIND_REQUIRES)
	return "Requires(auto):";
    return "Requires:";
}

static int
rpmGraph(rpmts ts, struct rpmInstallArguments_s * ia, const char ** fileArgv)
{
    rpmps ps;
    const char ** pkgURL = NULL;
    char * pkgState = NULL;
    const char ** fnp;
    const char * fileURL = NULL;
    int numPkgs = 0;
    int numFailed = 0;
    int prevx = 0;
    int pkgx = 0;
    const char ** argv = NULL;
    int argc = 0;
    const char ** av = NULL;
    int ac = 0;
    Header h;
    rpmRC rpmrc;
    int rc = 0;
    rpmVSFlags ovsflags;
    int i;

    if (fileArgv == NULL)
	return 0;

    if (ia->qva_flags & VERIFY_DIGEST)
	vsflags |= _RPMVSF_NODIGESTS;
    if (ia->qva_flags & VERIFY_SIGNATURE)
	vsflags |= _RPMVSF_NOSIGNATURES;
    ovsflags = rpmtsSetVSFlags(ts, vsflags);

    /* Build fully globbed list of arguments in argv[argc]. */
    for (fnp = fileArgv; *fnp; fnp++) {
	av = _free(av);
	ac = 0;
	rc = rpmGlob(*fnp, &ac, &av);
	if (rc || ac == 0) continue;

	argv = xrealloc(argv, (argc+2) * sizeof(*argv));
	memcpy(argv+argc, av, ac * sizeof(*av));
	argc += ac;
	argv[argc] = NULL;
    }
    av = _free(av);	ac = 0;

restart:
    /* Allocate sufficient storage for next set of args. */
    if (pkgx >= numPkgs) {
	numPkgs = pkgx + argc;
	pkgURL = xrealloc(pkgURL, (numPkgs + 1) * sizeof(*pkgURL));
	memset(pkgURL + pkgx, 0, ((argc + 1) * sizeof(*pkgURL)));
	pkgState = xrealloc(pkgState, (numPkgs + 1) * sizeof(*pkgState));
	memset(pkgState + pkgx, 0, ((argc + 1) * sizeof(*pkgState)));
    }

    /* Copy next set of args. */
    for (i = 0; i < argc; i++) {
	fileURL = _free(fileURL);
	fileURL = argv[i];
	argv[i] = NULL;

	pkgURL[pkgx] = fileURL;
	fileURL = NULL;
	pkgx++;
    }
    fileURL = _free(fileURL);

    /* Continue processing file arguments, building transaction set. */
    for (fnp = pkgURL+prevx; *fnp != NULL; fnp++, prevx++) {
	const char * fileName;
	FD_t fd;

	(void) urlPath(*fnp, &fileName);

	/* Try to read the header from a package file. */
	fd = Fopen(*fnp, "r.ufdio");
	if (fd == NULL || Ferror(fd)) {
	    rpmlog(RPMERR_OPEN, _("open of %s failed: %s\n"), *fnp,
			Fstrerror(fd));
	    if (fd) {
		Fclose(fd);
		fd = NULL;
	    }
	    numFailed++; *fnp = NULL;
	    continue;
	}

	/* Read the header, verifying signatures (if present). */
	ovsflags = rpmtsSetVSFlags(ts, vsflags);
	rpmrc = rpmReadPackageFile(ts, fd, *fnp, &h);
	ovsflags = rpmtsSetVSFlags(ts, ovsflags);
	Fclose(fd);
	fd = NULL;

	switch (rpmrc) {
	case RPMRC_FAIL:
	default:
	    rpmMessage(RPMMESS_ERROR, _("%s cannot be installed\n"), *fnp);
	    numFailed++; *fnp = NULL;
	    break;
	case RPMRC_OK:
	    rc = rpmtsAddInstallElement(ts, h, (fnpyKey)fileName, 0, NULL);
	    break;
	case RPMRC_NOTFOUND:
	    goto maybe_manifest;
	    break;
	}
	h = headerFree(h); 
	continue;

maybe_manifest:
	/* Try to read a package manifest. */
	fd = Fopen(*fnp, "r.fpio");
	if (fd == NULL || Ferror(fd)) {
	    rpmlog(RPMERR_OPEN, _("open of %s failed: %s\n"), *fnp,
			Fstrerror(fd));
	    if (fd) {
		Fclose(fd);
		fd = NULL;
	    }
	    numFailed++; *fnp = NULL;
	    break;
	}

	/* Read list of packages from manifest. */
	rc = rpmReadPackageManifest(fd, &argc, &argv);
	if (rc)
	    rpmlog(RPMERR_MANIFEST, _("%s: read manifest failed: %s\n"),
			fileURL, Fstrerror(fd));
	Fclose(fd);
	fd = NULL;

	/* If successful, restart the query loop. */
	if (rc == 0) {
	    prevx++;
	    goto restart;
	}

	numFailed++; *fnp = NULL;
	break;
    }

    if (numFailed > 0) goto exit;

    if (!noDeps) {
	rc = rpmtsCheck(ts);
	if (rc) {
	    numFailed += numPkgs;
	    goto exit;
	}
	ps = rpmtsProblems(ts);
	if (rpmpsNumProblems(ps) > 0) {
	    rpmMessage(RPMMESS_ERROR, _("Failed dependencies:\n"));
	    rpmpsPrint(NULL, ps);
	    numFailed += numPkgs;

	    if (ts->suggests != NULL && ts->nsuggests > 0) {
		rpmMessage(RPMMESS_NORMAL, _("    Suggested resolutions:\n"));
		for (i = 0; i < ts->nsuggests; i++) {
		    const char * str = ts->suggests[i];

		    if (str == NULL)
			break;

		    rpmMessage(RPMMESS_NORMAL, "\t%s\n", str);
		    ts->suggests[i] = NULL;
		    str = _free(str);
		}
		ts->suggests = _free(ts->suggests);
	    }
	}
	ps = rpmpsFree(ps);
    }

    rc = rpmtsOrder(ts);
    if (rc)
	goto exit;

    {	rpmtsi pi;
	rpmte p;
	rpmte q;
	unsigned char * selected =
			alloca(sizeof(*selected) * (rpmtsNElements(ts) + 1));
	int oType = TR_ADDED;

	fprintf(stdout, "digraph XXX {\n");

	fprintf(stdout, "  rankdir=LR\n");

	fprintf(stdout, "//===== Packages:\n");
	pi = rpmtsiInit(ts);
	while ((p = rpmtsiNext(pi, oType)) != NULL) {
	    fprintf(stdout, "//%5d%5d %s\n", rpmteTree(p), rpmteDepth(p), rpmteN(p));
	    q = rpmteParent(p);
	    if (q != NULL)
		fprintf(stdout, "  \"%s\" -> \"%s\"\n", rpmteN(p), rpmteN(q));
	    else {
		fprintf(stdout, "  \"%s\"\n", rpmteN(p));
		fprintf(stdout, "  { rank=max ; \"%s\" }\n", rpmteN(p));
	    }
	}
	pi = rpmtsiFree(pi);

	fprintf(stdout, "}\n");
    }

    rc = 0;

exit:
    for (i = 0; i < numPkgs; i++)
        pkgURL[i] = _free(pkgURL[i]);
    pkgState = _free(pkgState);
    pkgURL = _free(pkgURL);
    argv = _free(argv);

    return rc;
}

static struct poptOption optionsTable[] = {
 { "check", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &noDeps, 0,
	N_("don't verify package dependencies"), NULL },
 { "nolegacy", '\0', POPT_BIT_SET,	&vsflags, RPMVSF_NEEDPAYLOAD,
        N_("don't verify header+payload signature"), NULL },
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmcliAllPoptTable, 0,
	N_("Common options for all rpm modes and executables:"),
	NULL }, 

   POPT_AUTOALIAS
   POPT_AUTOHELP
   POPT_TABLEEND
};

int
main(int argc, char *argv[])
{
    rpmts ts = NULL;
    struct rpmInstallArguments_s * ia = &rpmIArgs;
    poptContext optCon;
    int ec = 0;

    optCon = rpmcliInit(argc, argv, optionsTable);
    if (optCon == NULL)
	exit(EXIT_FAILURE);

    ts = rpmtsCreate();
    if (rpmcliQueryFlags & VERIFY_DIGEST)
	vsflags |= _RPMVSF_NODIGESTS;
    if (rpmcliQueryFlags & VERIFY_SIGNATURE)
	vsflags |= _RPMVSF_NOSIGNATURES;
    if (rpmcliQueryFlags & VERIFY_HDRCHK)
	vsflags |= RPMVSF_NOHDRCHK;
    (void) rpmtsSetVSFlags(ts, vsflags);

    ec = rpmGraph(ts, ia, poptGetArgs(optCon));

    ts = rpmtsFree(ts);

    optCon = rpmcliFini(optCon);

    return ec;
}
