/** \ingroup rpmts
 * \file lib/depends.c
 */

#include "system.h"

#include <rpm/rpmcli.h>		/* XXX rpmcliPackagesTotal */

#include <rpm/rpmlib.h>		/* rpmVersionCompare, rpmlib provides */
#include <rpm/rpmtag.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmdb.h>
#include <rpm/rpmds.h>
#include <rpm/rpmfi.h>

#include "lib/rpmts_internal.h"
#include "lib/rpmte_internal.h"

#include "debug.h"

const char * const rpmNAME = PACKAGE;

const char * const rpmEVR = VERSION;

const int rpmFLAGS = RPMSENSE_EQUAL;

/* rpmlib provides */
static rpmds rpmlibP = NULL;

#undef HASHTYPE
#undef HTKEYTYPE
#undef HTDATATYPE

#define HASHTYPE depCache
#define HTKEYTYPE const char *
#define HTDATATYPE int
#include "lib/rpmhash.H"
#include "lib/rpmhash.C"

/**
 * Compare removed package instances (qsort/bsearch).
 * @param a		1st instance address
 * @param b		2nd instance address
 * @return		result of comparison
 */
static int intcmp(const void * a, const void * b)
{
    const int * aptr = a;
    const int * bptr = b;
    int rc = (*aptr - *bptr);
    return rc;
}

/**
 * Add removed package instance to ordered transaction set.
 * @param ts		transaction set
 * @param h		header
 * @param depends	installed package of pair (or RPMAL_NOMATCH on erase)
 * @return		0 on success
 */
static int removePackage(tsMembers tsmem, Header h, rpmte depends)
{
    rpmte p;
    unsigned int dboffset = headerGetInstance(h);

    /* Can't remove what's not installed */
    if (dboffset == 0) return 1;

    /* Filter out duplicate erasures. */
    if (tsmem->numRemovedPackages > 0 && tsmem->removedPackages != NULL) {
	if (bsearch(&dboffset,
		    tsmem->removedPackages, tsmem->numRemovedPackages,
		    sizeof(*tsmem->removedPackages), intcmp) != NULL)
	    return 0;
    }

    if (tsmem->numRemovedPackages == tsmem->allocedRemovedPackages) {
	tsmem->allocedRemovedPackages += tsmem->delta;
	tsmem->removedPackages = xrealloc(tsmem->removedPackages,
		sizeof(tsmem->removedPackages) * tsmem->allocedRemovedPackages);
    }

    if (tsmem->removedPackages != NULL) {	/* XXX can't happen. */
	tsmem->removedPackages[tsmem->numRemovedPackages] = dboffset;
	tsmem->numRemovedPackages++;
	if (tsmem->numRemovedPackages > 1)
	    qsort(tsmem->removedPackages, tsmem->numRemovedPackages,
			sizeof(*tsmem->removedPackages), intcmp);
    }

    if (tsmem->orderCount >= tsmem->orderAlloced) {
	tsmem->orderAlloced += (tsmem->orderCount - tsmem->orderAlloced) + tsmem->delta;
	tsmem->order = xrealloc(tsmem->order, sizeof(*tsmem->order) * tsmem->orderAlloced);
    }

    p = rpmteNew(NULL, h, TR_REMOVED, NULL, NULL, -1);
    rpmteSetDependsOn(p, depends);

    tsmem->order[tsmem->orderCount] = p;
    tsmem->orderCount++;

    return 0;
}

/* Return rpmdb iterator with removals pruned out */
static rpmdbMatchIterator rpmtsPrunedIterator(rpmts ts, rpmTag tag, const char * key)
{
    rpmdbMatchIterator mi = rpmtsInitIterator(ts, tag, key, 0);
    tsMembers tsmem = rpmtsMembers(ts);
    rpmdbPruneIterator(mi, tsmem->removedPackages, tsmem->numRemovedPackages,1);
    return mi;
}

#define skipColor(_tscolor, _color, _ocolor) \
	((_tscolor) && (_color) && (_ocolor) && !((_color) & (_ocolor)))

int rpmtsAddInstallElement(rpmts ts, Header h,
			fnpyKey key, int upgrade, rpmRelocation * relocs)
{
    tsMembers tsmem = rpmtsMembers(ts);
    rpm_color_t tscolor = rpmtsColor(ts);
    rpm_color_t hcolor;
    rpmdbMatchIterator mi;
    Header oh;
    int isSource = headerIsSource(h);
    int duplicate = 0;
    rpmtsi pi = NULL; rpmte p;
    rpmds oldChk = NULL, newChk = NULL, sameChk = NULL;
    rpmds obsoletes;
    int xx;
    int ec = 0;
    int rc;
    int oc;

    /* Check for supported payload format if it's a package */
    if (key && headerCheckPayloadFormat(h) != RPMRC_OK) {
	ec = 1;
	goto exit;
    }

    if (tsmem->addedPackages == NULL) {
	tsmem->addedPackages = rpmalCreate(5, tscolor, rpmtsPrefColor(ts));
    }

    /* XXX Always add source headers. */
    if (isSource) {
	oc = tsmem->orderCount;
	goto addheader;
    }

    /*
     * Check for previously added versions with the same name and arch/os.
     * FIXME: only catches previously added, older packages.
     */
    oldChk = rpmdsThis(h, RPMTAG_REQUIRENAME, (RPMSENSE_LESS));
    newChk = rpmdsThis(h, RPMTAG_REQUIRENAME, (RPMSENSE_GREATER));
    sameChk = rpmdsThis(h, RPMTAG_REQUIRENAME, (RPMSENSE_EQUAL));
    /* XXX can't use rpmtsiNext() filter or oc will have wrong value. */
    for (pi = rpmtsiInit(ts), oc = 0; (p = rpmtsiNext(pi, 0)) != NULL; oc++) {
	rpmds this;
	const char *pkgNEVR, *addNEVR;
	int skip = 0;

	/* XXX Only added packages need be checked for dupes. */
	if (rpmteType(p) == TR_REMOVED)
	    continue;

	/* XXX Never check source headers. */
	if (rpmteIsSource(p))
	    continue;

	if (tscolor) {
	    const char * arch = headerGetString(h, RPMTAG_ARCH);
	    const char * os = headerGetString(h, RPMTAG_OS);
	    const char * parch = rpmteA(p);
	    const char * pos = rpmteO(p);

	    if (arch == NULL || parch == NULL || os == NULL || pos == NULL)
		continue;
	    if (!rstreq(arch, parch) || !rstreq(os, pos))
		continue;
	}

	/* OK, binary rpm's with same arch and os.  Check NEVR. */
	if ((this = rpmteDS(p, RPMTAG_NAME)) == NULL)
	    continue;	/* XXX can't happen */

	/* 
	 * Always skip identical NEVR. 
 	 * On upgrade, if newer NEVR was previously added, 
 	 * then skip adding older. 
 	 */
	if (rpmdsCompare(sameChk, this)) {
	    skip = 1;
	    addNEVR = rpmdsDNEVR(sameChk);
	} else if (upgrade && rpmdsCompare(newChk, this)) {
	    skip = 1;
	    addNEVR = rpmdsDNEVR(newChk);
	}

	if (skip) {
	    pkgNEVR = rpmdsDNEVR(this);
	    if (rpmIsVerbose())
		rpmlog(RPMLOG_WARNING,
		    _("package %s was already added, skipping %s\n"),
		    (pkgNEVR ? pkgNEVR + 2 : "?pkgNEVR?"),
		    (addNEVR ? addNEVR + 2 : "?addNEVR?"));
	    ec = 0;
	    goto exit;
	}

	/*
 	 * On upgrade, if older NEVR was previously added, 
 	 * then replace old with new. 
 	 */
	rc = rpmdsCompare(oldChk, this);
	if (upgrade && rc != 0) {
	    pkgNEVR = rpmdsDNEVR(this);
	    addNEVR = rpmdsDNEVR(newChk);
	    if (rpmIsVerbose())
		rpmlog(RPMLOG_WARNING,
		    _("package %s was already added, replacing with %s\n"),
		    (pkgNEVR ? pkgNEVR + 2 : "?pkgNEVR?"),
		    (addNEVR ? addNEVR + 2 : "?addNEVR?"));
	    duplicate = 1;
	    rpmalDel(tsmem->addedPackages, p);
	    break;
	}
    }
    pi = rpmtsiFree(pi);

    /* If newer NEVR was already added, exit now. */
    if (ec)
	goto exit;

addheader:
    if (oc >= tsmem->orderAlloced) {
	tsmem->orderAlloced += (oc - tsmem->orderAlloced) + tsmem->delta;
	tsmem->order = xrealloc(tsmem->order,
			tsmem->orderAlloced * sizeof(*tsmem->order));
    }

    p = rpmteNew(NULL, h, TR_ADDED, key, relocs, -1);

    if (duplicate && oc < tsmem->orderCount) {
	tsmem->order[oc] = rpmteFree(tsmem->order[oc]);
    }

    tsmem->order[oc] = p;
    if (!duplicate) {
	tsmem->orderCount++;
	rpmcliPackagesTotal++;
    }
    
    rpmalAdd(tsmem->addedPackages, p);

    if (!duplicate) {
	tsmem->numAddedPackages++;
    }

    /* XXX rpmgi hack: Save header in transaction element if requested. */
    if (upgrade & 0x2)
	(void) rpmteSetHeader(p, h);

    /* If not upgrading or a source package, then we're done. */
    if (!(upgrade & 0x1) || isSource)
	goto exit;

    /* Do lazy (readonly?) open of rpm database. */
    if (rpmtsGetRdb(ts) == NULL && rpmtsGetDBMode(ts) != -1) {
	if ((ec = rpmtsOpenDB(ts, rpmtsGetDBMode(ts))) != 0)
	    goto exit;
    }

    /* On upgrade, erase older packages of same color (if any). */
    hcolor = rpmteColor(p);
    mi = rpmtsInitIterator(ts, RPMTAG_NAME, rpmteN(p), 0);
    while((oh = rpmdbNextIterator(mi)) != NULL) {
	/* Ignore colored packages not in our rainbow. */
	if (skipColor(tscolor, hcolor, headerGetNumber(oh, RPMTAG_HEADERCOLOR)))
	    continue;

	/* Skip packages that contain identical NEVR. */
	if (rpmVersionCompare(h, oh) == 0)
	    continue;

	xx = removePackage(tsmem, oh, p);
    }
    mi = rpmdbFreeIterator(mi);

    obsoletes = rpmdsInit(rpmteDS(p, RPMTAG_OBSOLETENAME));
    while (rpmdsNext(obsoletes) >= 0) {
	const char * Name;

	if ((Name = rpmdsN(obsoletes)) == NULL)
	    continue;	/* XXX can't happen */

	/* XXX avoid self-obsoleting packages. */
	if (rstreq(rpmteN(p), Name))
	    continue;

	mi = rpmtsPrunedIterator(ts, RPMTAG_NAME, Name);

	while((oh = rpmdbNextIterator(mi)) != NULL) {
	    /* Ignore colored packages not in our rainbow. */
	    if (skipColor(tscolor, hcolor, 
			  headerGetNumber(oh, RPMTAG_HEADERCOLOR)))
		continue;

	    /*
	     * Rpm prior to 3.0.3 does not have versioned obsoletes.
	     * If no obsoletes version info is available, match all names.
	     */
	    if (rpmdsEVR(obsoletes) == NULL
	     || rpmdsAnyMatchesDep(oh, obsoletes, _rpmds_nopromote)) {
		char * ohNEVRA = headerGetAsString(oh, RPMTAG_NEVRA);
		rpmlog(RPMLOG_DEBUG, "  Obsoletes: %s\t\terases %s\n",
			rpmdsDNEVR(obsoletes)+2, ohNEVRA);
		ohNEVRA = _free(ohNEVRA);

		removePackage(tsmem, oh, p);
	    }
	}
	mi = rpmdbFreeIterator(mi);
    }

    ec = 0;

exit:
    oldChk = rpmdsFree(oldChk);
    newChk = rpmdsFree(newChk);
    sameChk = rpmdsFree(sameChk);
    pi = rpmtsiFree(pi);
    return ec;
}

int rpmtsAddEraseElement(rpmts ts, Header h, int dboffset)
{
    return removePackage(rpmtsMembers(ts), h, NULL);
}

/**
 * Check dep for an unsatisfied dependency.
 * @param ts		transaction set
 * @param dep		dependency
 * @param adding	dependency is from added package set?
 * @return		0 if satisfied, 1 if not satisfied
 */
static int unsatisfiedDepend(rpmts ts, depCache dcache, rpmds dep, int adding)
{
    tsMembers tsmem = rpmtsMembers(ts);
    rpmdbMatchIterator mi;
    const char * Name = rpmdsN(dep);
    const char * DNEVR = rpmdsDNEVR(dep);
    Header h;
    int rc;
    int xx;
    int retrying = 0;
    int *cachedrc = NULL;
    int cacheThis = 0;

retry:
    rc = 0;	/* assume dependency is satisfied */

    /*
     * New features in rpm packaging implicitly add versioned dependencies
     * on rpmlib provides. The dependencies look like "rpmlib(YaddaYadda)".
     * Check those dependencies now.
     */
    if (rpmdsFlags(dep) & RPMSENSE_RPMLIB) {
	static int oneshot = -1;
	if (oneshot) 
	    oneshot = rpmdsRpmlib(&rpmlibP, NULL);
	
	if (rpmlibP != NULL && rpmdsSearch(rpmlibP, dep) >= 0) {
	    rpmdsNotify(dep, _("(rpmlib provides)"), rc);
	    goto exit;
	}
	goto unsatisfied;
    }

    /* Search added packages for the dependency. */
    if (rpmalSatisfiesDepend(tsmem->addedPackages, dep) != NULL) {
	goto exit;
    }

    /* See if we already looked this up */
    if (depCacheGetEntry(dcache, DNEVR, &cachedrc, NULL, NULL)) {
	rc = *cachedrc;
	rpmdsNotify(dep, _("(cached)"), rc);
	return rc;
    }
    /* Only bother caching the expensive rpmdb lookups */
    cacheThis = 1;

    if (Name[0] == '/') {
	mi = rpmtsPrunedIterator(ts, RPMTAG_BASENAMES, Name);

	while ((h = rpmdbNextIterator(mi)) != NULL) {
	    rpmdsNotify(dep, _("(db files)"), rc);
	    mi = rpmdbFreeIterator(mi);
	    goto exit;
	}
	mi = rpmdbFreeIterator(mi);
    }

    mi = rpmtsPrunedIterator(ts, RPMTAG_PROVIDENAME, Name);
    while ((h = rpmdbNextIterator(mi)) != NULL) {
	if (rpmdsAnyMatchesDep(h, dep, _rpmds_nopromote)) {
	    rpmdsNotify(dep, _("(db provides)"), rc);
	    mi = rpmdbFreeIterator(mi);
	    goto exit;
	}
    }
    mi = rpmdbFreeIterator(mi);

    /*
     * Search for an unsatisfied dependency.
     */
    if (adding && !retrying && !(rpmtsFlags(ts) & RPMTRANS_FLAG_NOSUGGEST)) {
	xx = rpmtsSolve(ts, dep);
	if (xx == 0)
	    goto exit;
	if (xx == -1) {
	    retrying = 1;
	    goto retry;
	}
    }

unsatisfied:
    rc = 1;	/* dependency is unsatisfied */
    rpmdsNotify(dep, NULL, rc);

exit:
    if (cacheThis) {
	char *key = xstrdup(DNEVR);
	depCacheAddEntry(dcache, key, rc);
    }
	
    return rc;
}

/* Check a dependency set for problems */
static void checkDS(rpmts ts, depCache dcache, rpmte te,
		const char * pkgNEVRA, rpmds ds,
		const char * depName, rpm_color_t tscolor, int adding)
{
    rpm_color_t dscolor;
    /* require-problems are unsatisfied, others appear "satisfied" */
    int is_problem = (rpmdsTagN(ds) == RPMTAG_REQUIRENAME);

    ds = rpmdsInit(ds);
    while (rpmdsNext(ds) >= 0) {
	/* Filter out dependencies that came along for the ride. */
	if (depName != NULL && !rstreq(depName, rpmdsN(ds)))
	    continue;

	/* Ignore colored dependencies not in our rainbow. */
	dscolor = rpmdsColor(ds);
	if (tscolor && dscolor && !(tscolor & dscolor))
	    continue;

	if (unsatisfiedDepend(ts, dcache, ds, adding) == is_problem)
	    rpmteAddDepProblem(te, pkgNEVRA, ds, NULL, adding);
    }
}

/**
 * Check dependency against installed packages.
 * Adding: check name/provides dep against each conflict match,
 * Erasing: check name/provides/filename dep against each requiredby match.
 * @param ts		transaction set
 * @param dep		dependency name
 * @param mi		rpm database iterator
 * @param adding	dependency is from added package set?
 */
static void checkPackageSet(rpmts ts, depCache dcache, rpmte te,
			const char * dep, rpmdbMatchIterator mi, int adding)
{
    Header h;

    while ((h = rpmdbNextIterator(mi)) != NULL) {
	char * pkgNEVRA = headerGetAsString(h, RPMTAG_NEVRA);
	rpmds requires = rpmdsNew(h, RPMTAG_REQUIRENAME, 0);
	rpmds conflicts = rpmdsNew(h, RPMTAG_CONFLICTNAME, 0);
	rpmds obsoletes = rpmdsNew(h, RPMTAG_OBSOLETENAME, 0);

	checkDS(ts, dcache, te, pkgNEVRA, requires, dep, 0, adding);
	checkDS(ts, dcache, te, pkgNEVRA, conflicts, dep, 0, adding);
	checkDS(ts, dcache, te, pkgNEVRA, obsoletes, dep, 0, adding);

	conflicts = rpmdsFree(conflicts);
	requires = rpmdsFree(requires);
	obsoletes = rpmdsFree(requires);
	pkgNEVRA = _free(pkgNEVRA);
    }
}

/* Check a given dependency type against installed packages */
static void checkInstDeps(rpmts ts, depCache dcache, rpmte te,
			  rpmTag depTag, const char *dep)
{
    rpmdbMatchIterator mi = rpmtsPrunedIterator(ts, depTag, dep);
    checkPackageSet(ts, dcache, te, dep, mi, 0);
    rpmdbFreeIterator(mi);
}

int rpmtsCheck(rpmts ts)
{
    rpm_color_t tscolor = rpmtsColor(ts);
    rpmtsi pi = NULL; rpmte p;
    int closeatexit = 0;
    int rc;
    depCache dcache = NULL;
    
    (void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_CHECK), 0);

    /* Do lazy, readonly, open of rpm database. */
    if (rpmtsGetRdb(ts) == NULL && rpmtsGetDBMode(ts) != -1) {
	if ((rc = rpmtsOpenDB(ts, rpmtsGetDBMode(ts))) != 0)
	    goto exit;
	closeatexit = 1;
    }

    /* XXX FIXME: figure some kind of heuristic for the cache size */
    dcache = depCacheCreate(5001, hashFunctionString, strcmp,
				     (depCacheFreeKey)rfree, NULL);

    /*
     * Look at all of the added packages and make sure their dependencies
     * are satisfied.
     */
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, TR_ADDED)) != NULL) {
	rpmds provides = rpmdsInit(rpmteDS(p, RPMTAG_PROVIDENAME));

	rpmlog(RPMLOG_DEBUG, "========== +++ %s %s/%s 0x%x\n",
		rpmteNEVR(p), rpmteA(p), rpmteO(p), rpmteColor(p));

	checkDS(ts, dcache, p, rpmteNEVRA(p), rpmteDS(p, RPMTAG_REQUIRENAME),
		NULL, tscolor, 1);
	checkDS(ts, dcache, p, rpmteNEVRA(p), rpmteDS(p, RPMTAG_CONFLICTNAME),
		NULL, tscolor, 1);

	/* Check provides against conflicts in installed packages. */
	while (rpmdsNext(provides) >= 0) {
	    checkInstDeps(ts, dcache, p, RPMTAG_CONFLICTNAME, rpmdsN(provides));
	}

	/* Check package name (not provides!) against installed obsoletes */
	checkInstDeps(ts, dcache, p, RPMTAG_OBSOLETENAME, rpmteN(p));
    }
    pi = rpmtsiFree(pi);

    /*
     * Look at the removed packages and make sure they aren't critical.
     */
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, TR_REMOVED)) != NULL) {
	rpmds provides = rpmdsInit(rpmteDS(p, RPMTAG_PROVIDENAME));
	rpmfi fi = rpmfiInit(rpmteFI(p), 0);

	rpmlog(RPMLOG_DEBUG, "========== --- %s %s/%s 0x%x\n",
		rpmteNEVR(p), rpmteA(p), rpmteO(p), rpmteColor(p));

	/* Check provides and filenames against installed dependencies. */
	while (rpmdsNext(provides) >= 0) {
	    checkInstDeps(ts, dcache, p, RPMTAG_REQUIRENAME, rpmdsN(provides));
	}

	while (rpmfiNext(fi) >= 0) {
	    checkInstDeps(ts, dcache, p, RPMTAG_REQUIRENAME, rpmfiFN(fi));
	}
    }
    pi = rpmtsiFree(pi);

exit:
    depCacheFree(dcache);

    (void) rpmswExit(rpmtsOp(ts, RPMTS_OP_CHECK), 0);

    if (closeatexit)
	(void) rpmtsCloseDB(ts);
    return rc;
}
