#include "system.h"

#include <rpmlib.h>

#include "depends.h"
#include "rpmdb.h"
#include "misc.h"

/*@access rpmTransactionSet@*/

int headerNVR(Header h, const char **np, const char **vp, const char **rp)
{
    int type, count;
    if (np && !headerGetEntry(h, RPMTAG_NAME, &type, (void **) np, &count))
	*np = NULL;
    if (vp && !headerGetEntry(h, RPMTAG_VERSION, &type, (void **) vp, &count))
	*vp = NULL;
    if (rp && !headerGetEntry(h, RPMTAG_RELEASE, &type, (void **) rp, &count))
	*rp = NULL;
    return 0;
}

static /*@only@*/ char *printDepend(const char * depend, const char * key, const char * keyEVR,
	int keyFlags)
{
    char *tbuf, *t;
    size_t nb;

    nb = 0;
    if (depend)	nb += strlen(depend) + 1;
    if (key)	nb += strlen(key);
    if (keyFlags & RPMSENSE_SENSEMASK) {
	if (nb)	nb++;
	if (keyFlags & RPMSENSE_LESS)	nb++;
	if (keyFlags & RPMSENSE_GREATER) nb++;
	if (keyFlags & RPMSENSE_EQUAL)	nb++;
    }
    if (keyEVR && *keyEVR) {
	if (nb)	nb++;
	nb += strlen(keyEVR);
    }

    t = tbuf = xmalloc(nb + 1);
    if (depend) {
	while(*depend)	*t++ = *depend++;
	*t++ = ' ';
    }
    if (key)
	while(*key)	*t++ = *key++;
    if (keyFlags & RPMSENSE_SENSEMASK) {
	if (t != tbuf)	*t++ = ' ';
	if (keyFlags & RPMSENSE_LESS)	*t++ = '<';
	if (keyFlags & RPMSENSE_GREATER) *t++ = '>';
	if (keyFlags & RPMSENSE_EQUAL)	*t++ = '=';
    }
    if (keyEVR && *keyEVR) {
	if (t != tbuf)	*t++ = ' ';
	while(*keyEVR)	*t++ = *keyEVR++;
    }
    *t = '\0';
    return tbuf;
}

#ifdef	UNUSED
static /*@only@*/ const char *buildEVR(int_32 *e, const char *v, const char *r)
{
    const char *pEVR;
    char *p;

    pEVR = p = xmalloc(21 + strlen(v) + 1 + strlen(r) + 1);
    *p = '\0';
    if (e) {
	sprintf(p, "%d:", *e);
	while (*p)
	    p++;
    }
    (void) stpcpy( stpcpy( stpcpy(p, v) , "-") , r);
    return pEVR;
}
#endif

struct orderListIndex {
    int alIndex;
    int orIndex;
};

static void alFreeIndex(struct availableList * al)
{
    if (al->index.size) {
	if (al->index.index)
		free(al->index.index);
	al->index.index = NULL;
	al->index.size = 0;
    }
}

static void alCreate( /*@out@*/ struct availableList * al)
{
    al->alloced = 5;
    al->size = 0;
    al->list = xcalloc(al->alloced, sizeof(*al->list));

    al->index.index = NULL;
    al->index.size = 0;

    al->numDirs = 0;
    al->dirs = NULL;
}

static void alFree(struct availableList * al)
{
    int i;
    rpmRelocation * r;

    for (i = 0; i < al->size; i++) {
	if (al->list[i].provides)
	    free(al->list[i].provides);
	if (al->list[i].providesEVR)
	    free(al->list[i].providesEVR);
	if (al->list[i].baseNames)
	    free(al->list[i].baseNames);
	headerFree(al->list[i].h);

	if (al->list[i].relocs) {
	    for (r = al->list[i].relocs; (r->oldPath || r->newPath); r++) {
		if (r->oldPath) xfree(r->oldPath);
		if (r->newPath) xfree(r->newPath);
	    }
	    free(al->list[i].relocs);
	}
	if (al->list[i].fd)
	    al->list[i].fd = fdFree(al->list[i].fd, "alAddPackage (alFree)");
    }

    for (i = 0; i < al->numDirs; i++) {
	free(al->dirs[i].dirName);
	free(al->dirs[i].files);
    }

    if (al->numDirs)
	free(al->dirs);
    al->dirs = NULL;

    if (al->alloced && al->list)
	free(al->list);
    al->list = NULL;
    alFreeIndex(al);
}

static int dirInfoCompare(const void * one, const void * two) {
    const struct dirInfo * a = one;
    const struct dirInfo * b = two;
    int lenchk = a->dirNameLen - b->dirNameLen;

    if (lenchk)
	return lenchk;

    /* XXX FIXME: this might do "backward" strcmp for speed */
    return strcmp(a->dirName, b->dirName);
}

static /*@exposed@*/ struct availablePackage * alAddPackage(struct availableList * al,
		Header h, /*@dependent@*/ const void * key,
		FD_t fd, rpmRelocation * relocs)
{
    struct availablePackage * p;
    rpmRelocation * r;
    int i;
    int_32 * dirIndexes;
    const char ** dirNames;
    int numDirs, dirNum;
    int * dirMapping;
    struct dirInfo dirNeedle;
    struct dirInfo * dirMatch;
    int first, last, fileNum;
    int origNumDirs;
    int pkgNum;
    uint_32 multiLibMask = 0;
    uint_32 * fileFlags = NULL;
    uint_32 * pp = NULL;

    if (al->size == al->alloced) {
	al->alloced += 5;
	al->list = xrealloc(al->list, sizeof(*al->list) * al->alloced);
    }

    pkgNum = al->size++;
    p = al->list + pkgNum;
    p->h = headerLink(h);	/* XXX reference held by transaction set */
    p->multiLib = 0;	/* MULTILIB */

    headerNVR(p->h, &p->name, &p->version, &p->release);

    /* XXX This should be added always so that packages look alike.
     * XXX However, there is logic in files.c/depends.c that checks for
     * XXX existence (rather than value) that will need to change as well.
     */
    if (headerGetEntry(p->h, RPMTAG_MULTILIBS, NULL, (void **) &pp, NULL))
	multiLibMask = *pp;

    if (multiLibMask) {
	for (i = 0; i < pkgNum - 1; i++) {
	    if (!strcmp (p->name, al->list[i].name)
		&& headerGetEntry(al->list[i].h, RPMTAG_MULTILIBS, NULL,
				  (void **) &pp, NULL)
		&& !rpmVersionCompare(p->h, al->list[i].h)
		&& *pp && !(*pp & multiLibMask))
		p->multiLib = multiLibMask;
	}
    }

    if (!headerGetEntry(h, RPMTAG_EPOCH, NULL, (void **) &p->epoch, NULL))
	p->epoch = NULL;

    if (!headerGetEntry(h, RPMTAG_PROVIDENAME, NULL, (void **) &p->provides,
	&p->providesCount)) {
	p->providesCount = 0;
	p->provides = NULL;
	p->providesEVR = NULL;
	p->provideFlags = NULL;
    } else {
	if (!headerGetEntry(h, RPMTAG_PROVIDEVERSION,
			NULL, (void **) &p->providesEVR, NULL))
	    p->providesEVR = NULL;
	if (!headerGetEntry(h, RPMTAG_PROVIDEFLAGS,
			NULL, (void **) &p->provideFlags, NULL))
	    p->provideFlags = NULL;
    }

    if (!headerGetEntryMinMemory(h, RPMTAG_BASENAMES, NULL, (void **) 
				 &p->baseNames, &p->filesCount)) {
	p->filesCount = 0;
	p->baseNames = NULL;
    } else {
        headerGetEntryMinMemory(h, RPMTAG_DIRNAMES, NULL, (void **) 
				 &dirNames, &numDirs);
        headerGetEntryMinMemory(h, RPMTAG_DIRINDEXES, NULL, (void **) 
				 &dirIndexes, NULL);
	headerGetEntry(h, RPMTAG_FILEFLAGS, NULL, (void **) &fileFlags, NULL);

	/* XXX FIXME: We ought to relocate the directory list here */

        dirMapping = alloca(sizeof(*dirMapping) * numDirs);

	/* allocated enough space for all the directories we could possible
	   need to add */
	al->dirs = xrealloc(al->dirs, 
			    sizeof(*al->dirs) * (al->numDirs + numDirs));
	origNumDirs = al->numDirs;

	for (dirNum = 0; dirNum < numDirs; dirNum++) {
	    dirNeedle.dirName = (char *) dirNames[dirNum];
	    dirNeedle.dirNameLen = strlen(dirNames[dirNum]);
	    dirMatch = bsearch(&dirNeedle, al->dirs, origNumDirs,
			       sizeof(dirNeedle), dirInfoCompare);
	    if (dirMatch) {
		dirMapping[dirNum] = dirMatch - al->dirs;
	    } else {
		dirMapping[dirNum] = al->numDirs;
		al->dirs[al->numDirs].dirName = xstrdup(dirNames[dirNum]);
		al->dirs[al->numDirs].dirNameLen = strlen(dirNames[dirNum]);
		al->dirs[al->numDirs].files = NULL;
		al->dirs[al->numDirs].numFiles = 0;
		al->numDirs++;
	    }
	}

	free(dirNames);

	first = 0;
	while (first < p->filesCount) {
	    last = first;
	    while ((last + 1) < p->filesCount) {
		if (dirIndexes[first] != dirIndexes[last + 1]) break;
		last++;
	    }

	    dirMatch = al->dirs + dirMapping[dirIndexes[first]];
	    dirMatch->files = xrealloc(dirMatch->files,
		sizeof(*dirMatch->files) * 
		    (dirMatch->numFiles + last - first + 1));
	    for (fileNum = first; fileNum <= last; fileNum++) {
		dirMatch->files[dirMatch->numFiles].baseName =
		    p->baseNames[fileNum];
		dirMatch->files[dirMatch->numFiles].pkgNum = pkgNum;
		dirMatch->files[dirMatch->numFiles].fileFlags =
				fileFlags[fileNum];
		dirMatch->numFiles++;
	    }

	    first = last + 1;
	}

	if (origNumDirs + al->numDirs)
	    qsort(al->dirs, al->numDirs, sizeof(dirNeedle), dirInfoCompare);

    }

    p->key = key;
    p->fd = (fd ? fdLink(fd, "alAddPackage") : NULL);

    if (relocs) {
	for (i = 0, r = relocs; r->oldPath || r->newPath; i++, r++);
	p->relocs = xmalloc(sizeof(*p->relocs) * (i + 1));

	for (i = 0, r = relocs; r->oldPath || r->newPath; i++, r++) {
	    p->relocs[i].oldPath = r->oldPath ? xstrdup(r->oldPath) : NULL;
	    p->relocs[i].newPath = r->newPath ? xstrdup(r->newPath) : NULL;
	}
	p->relocs[i].oldPath = NULL;
	p->relocs[i].newPath = NULL;
    } else {
	p->relocs = NULL;
    }

    alFreeIndex(al);

    return p;
}

static int indexcmp(const void * one, const void * two)
{
    const struct availableIndexEntry * a = one;
    const struct availableIndexEntry * b = two;
    int lenchk = a->entryLen - b->entryLen;

    if (lenchk)
	return lenchk;

    return strcmp(a->entry, b->entry);
}

static void alMakeIndex(struct availableList * al)
{
    struct availableIndex * ai = &al->index;
    int i, j, k;

    if (ai->size) return;

    for (i = 0; i < al->size; i++) 
	ai->size += al->list[i].providesCount;

    if (ai->size) {
	ai->index = xcalloc(ai->size, sizeof(*ai->index));

	k = 0;
	for (i = 0; i < al->size; i++) {
	    for (j = 0; j < al->list[i].providesCount; j++) {

		/* If multilib install, skip non-multilib provides. */
		if (al->list[i].multiLib &&
		    !isDependsMULTILIB(al->list[i].provideFlags[j])) {
			ai->size--;
			continue;
		}

		ai->index[k].package = al->list + i;
		ai->index[k].entry = al->list[i].provides[j];
		ai->index[k].entryLen = strlen(al->list[i].provides[j]);
		ai->index[k].type = IET_PROVIDES;
		k++;
	    }
	}

	qsort(ai->index, ai->size, sizeof(*ai->index), indexcmp);
    }
}

static int intcmp(const void * a, const void *b)
{
    const int * aptr = a;
    const int * bptr = b;
    int rc = (*aptr - *bptr);
    return rc;
}

static void parseEVR(char *evr,
	/*@exposed@*/ /*@out@*/ const char **ep,
	/*@exposed@*/ /*@out@*/ const char **vp,
	/*@exposed@*/ /*@out@*/const char **rp) /*@modifies evr,*ep,*vp,*rp @*/
{
    const char *epoch;
    const char *version;		/* assume only version is present */
    const char *release;
    char *s, *se;

    s = evr;
    while (*s && isdigit(*s)) s++;	/* s points to epoch terminator */
    se = strrchr(s, '-');		/* se points to version terminator */

    if (*s == ':') {
	epoch = evr;
	*s++ = '\0';
	version = s;
	if (*epoch == '\0') epoch = "0";
    } else {
	epoch = NULL;	/* XXX disable epoch compare if missing */
	version = evr;
    }
    if (se) {
	*se++ = '\0';
	release = se;
    } else {
	release = NULL;
    }

    if (ep) *ep = epoch;
    if (vp) *vp = version;
    if (rp) *rp = release;
}

const char *rpmNAME = PACKAGE;
const char *rpmEVR = VERSION;
int rpmFLAGS = RPMSENSE_EQUAL;

int rpmRangesOverlap(const char *AName, const char *AEVR, int AFlags,
	const char *BName, const char *BEVR, int BFlags)
{
    const char *aDepend = printDepend(NULL, AName, AEVR, AFlags);
    const char *bDepend = printDepend(NULL, BName, BEVR, BFlags);
    char *aEVR, *bEVR;
    const char *aE, *aV, *aR, *bE, *bV, *bR;
    int result;
    int sense;

    /* Different names don't overlap. */
    if (strcmp(AName, BName)) {
	result = 0;
	goto exit;
    }

    /* Same name. If either A or B is an existence test, always overlap. */
    if (!((AFlags & RPMSENSE_SENSEMASK) && (BFlags & RPMSENSE_SENSEMASK))) {
	result = 1;
	goto exit;
    }

    /* If either EVR is non-existent or empty, always overlap. */
    if (!(AEVR && *AEVR && BEVR && *BEVR)) {
	result = 1;
	goto exit;
    }

    /* Both AEVR and BEVR exist. */
    aEVR = xstrdup(AEVR);
    parseEVR(aEVR, &aE, &aV, &aR);
    bEVR = xstrdup(BEVR);
    parseEVR(bEVR, &bE, &bV, &bR);

    /* Compare {A,B} [epoch:]version[-release] */
    sense = 0;
    if (aE && *aE && bE && *bE)
	sense = rpmvercmp(aE, bE);
    else if (aE && *aE && atol(aE) > 0) {
	/* XXX legacy epoch-less requires/conflicts compatibility */
	rpmMessage(RPMMESS_DEBUG, _("the \"B\" dependency needs an epoch (assuming same as \"A\")\n\tA %s\tB %s\n"),
		aDepend, bDepend);
	sense = 0;
    } else if (bE && *bE && atol(bE) > 0)
	sense = -1;

    if (sense == 0) {
	sense = rpmvercmp(aV, bV);
	if (sense == 0 && aR && *aR && bR && *bR) {
	    sense = rpmvercmp(aR, bR);
	}
    }
    free(aEVR);
    free(bEVR);

    /* Detect overlap of {A,B} range. */
    result = 0;
    if (sense < 0 && ((AFlags & RPMSENSE_GREATER) || (BFlags & RPMSENSE_LESS))) {
	result = 1;
    } else if (sense > 0 && ((AFlags & RPMSENSE_LESS) || (BFlags & RPMSENSE_GREATER))) {
	result = 1;
    } else if (sense == 0 &&
	(((AFlags & RPMSENSE_EQUAL) && (BFlags & RPMSENSE_EQUAL)) ||
	 ((AFlags & RPMSENSE_LESS) && (BFlags & RPMSENSE_LESS)) ||
	 ((AFlags & RPMSENSE_GREATER) && (BFlags & RPMSENSE_GREATER)))) {
	result = 1;
    }

exit:
    rpmMessage(RPMMESS_DEBUG, _("  %s    A %s\tB %s\n"),
	(result ? "YES" : "NO "), aDepend, bDepend);
    if (aDepend) xfree(aDepend);
    if (bDepend) xfree(bDepend);
    return result;
}

typedef int (*dbrecMatch_t) (Header h, const char *reqName, const char * reqEVR, int reqFlags);

static int rangeMatchesDepFlags (Header h, const char *reqName, const char * reqEVR, int reqFlags)
{
    const char ** provides;
    const char ** providesEVR;
    int_32 * provideFlags;
    int providesCount;
    int result;
    int type;
    int i;

    if (!(reqFlags & RPMSENSE_SENSEMASK) || !reqEVR || !strlen(reqEVR))
	return 1;

    /* Get provides information from header */
    /*
     * Rpm prior to 3.0.3 does not have versioned provides.
     * If no provides version info is available, match any requires.
     */
    if (!headerGetEntry(h, RPMTAG_PROVIDEVERSION, &type,
		(void **) &providesEVR, &providesCount))
	return 1;

    headerGetEntry(h, RPMTAG_PROVIDEFLAGS, &type,
	(void **) &provideFlags, &providesCount);

    if (!headerGetEntry(h, RPMTAG_PROVIDENAME, &type,
		(void **) &provides, &providesCount)) {
	if (providesEVR) xfree(providesEVR);
	return 0;	/* XXX should never happen */
    }

    result = 0;
    for (i = 0; i < providesCount; i++) {

	/* Filter out provides that came along for the ride. */
	if (strcmp(provides[i], reqName))
	    continue;

	result = rpmRangesOverlap(provides[i], providesEVR[i], provideFlags[i],
			reqName, reqEVR, reqFlags);

	/* If this provide matches the require, we're done. */
	if (result)
	    break;
    }

    if (provides) xfree(provides);
    if (providesEVR) xfree(providesEVR);

    return result;
}

/* XXX FIXME: eliminate when obsoletes is correctly implemented */
int headerMatchesDepFlags(Header h,
	const char * reqName, const char * reqEVR, int reqFlags)
{
    const char *name, *version, *release;
    int_32 * epoch;
    const char *pkgEVR;
    char *p;
    int pkgFlags = RPMSENSE_EQUAL;

    if (!((reqFlags & RPMSENSE_SENSEMASK) && reqEVR && *reqEVR))
	return 1;

    /* Get package information from header */
    headerNVR(h, &name, &version, &release);

    pkgEVR = p = alloca(21 + strlen(version) + 1 + strlen(release) + 1);
    *p = '\0';
    if (headerGetEntry(h, RPMTAG_EPOCH, NULL, (void **) &epoch, NULL)) {
	sprintf(p, "%d:", *epoch);
	while (*p)
	    p++;
    }
    (void) stpcpy( stpcpy( stpcpy(p, version) , "-") , release);

    return rpmRangesOverlap(name, pkgEVR, pkgFlags, reqName, reqEVR, reqFlags);

}

rpmTransactionSet rpmtransCreateSet(rpmdb rpmdb, const char * rootDir)
{
    rpmTransactionSet rpmdep;
    int rootLen;

    if (!rootDir) rootDir = "";

    rpmdep = xcalloc(1, sizeof(*rpmdep));
    rpmdep->rpmdb = rpmdb;
    rpmdep->scriptFd = NULL;
    rpmdep->numRemovedPackages = 0;
    rpmdep->allocedRemovedPackages = 5;
    rpmdep->removedPackages = xcalloc(rpmdep->allocedRemovedPackages,
			sizeof(*rpmdep->removedPackages));

    /* This canonicalizes the root */
    rootLen = strlen(rootDir);
    if (!(rootLen && rootDir[rootLen - 1] == '/')) {
	char * t;

	t = alloca(rootLen + 2);
	*t = '\0';
	(void) stpcpy( stpcpy(t, rootDir), "/");
	rootDir = t;
    }

    rpmdep->rootDir = xstrdup(rootDir);
    rpmdep->currDir = NULL;

    alCreate(&rpmdep->addedPackages);
    alCreate(&rpmdep->availablePackages);

    rpmdep->orderAlloced = 5;
    rpmdep->orderCount = 0;
    rpmdep->order = xcalloc(rpmdep->orderAlloced, sizeof(*rpmdep->order));

    return rpmdep;
}

static void removePackage(rpmTransactionSet rpmdep, int dboffset, int depends)
{
    if (rpmdep->numRemovedPackages == rpmdep->allocedRemovedPackages) {
	rpmdep->allocedRemovedPackages += 5;
	rpmdep->removedPackages = xrealloc(rpmdep->removedPackages,
		sizeof(int *) * rpmdep->allocedRemovedPackages);
    }

    rpmdep->removedPackages[rpmdep->numRemovedPackages++] = dboffset;

    if (rpmdep->orderCount == rpmdep->orderAlloced) {
	rpmdep->orderAlloced += 5;
	rpmdep->order = xrealloc(rpmdep->order,
		sizeof(*rpmdep->order) * rpmdep->orderAlloced);
    }

    rpmdep->order[rpmdep->orderCount].type = TR_REMOVED;
    rpmdep->order[rpmdep->orderCount].u.removed.dboffset = dboffset;
    rpmdep->order[rpmdep->orderCount++].u.removed.dependsOnIndex = depends;
}

int rpmtransAddPackage(rpmTransactionSet rpmdep, Header h, FD_t fd,
			const void * key, int upgrade, rpmRelocation * relocs)
{
    /* this is an install followed by uninstalls */
    const char * name;
    int count;
    const char ** obsoletes;
    int alNum;

    /* XXX binary rpms always have RPMTAG_SOURCERPM, source rpms do not */
    if (headerIsEntry(h, RPMTAG_SOURCEPACKAGE))
	return 1;

    /* FIXME: handling upgrades like this is *almost* okay. It doesn't
       check to make sure we're upgrading to a newer version, and it
       makes it difficult to generate a return code based on the number of
       packages which failed. */

    if (rpmdep->orderCount == rpmdep->orderAlloced) {
	rpmdep->orderAlloced += 5;
	rpmdep->order = xrealloc(rpmdep->order,
		sizeof(*rpmdep->order) * rpmdep->orderAlloced);
    }
    rpmdep->order[rpmdep->orderCount].type = TR_ADDED;
    alNum = alAddPackage(&rpmdep->addedPackages, h, key, fd, relocs) -
    		rpmdep->addedPackages.list;
    rpmdep->order[rpmdep->orderCount++].u.addedIndex = alNum;

    if (!upgrade || rpmdep->rpmdb == NULL) return 0;

    headerNVR(h, &name, NULL, NULL);

    {	rpmdbMatchIterator mi;
	Header h2;

	mi = rpmdbInitIterator(rpmdep->rpmdb, RPMTAG_NAME, name, 0);
	while((h2 = rpmdbNextIterator(mi)) != NULL) {
	    if (rpmVersionCompare(h, h2))
		removePackage(rpmdep, rpmdbGetIteratorOffset(mi), alNum);
	    else {
		uint_32 *p, multiLibMask = 0, oldmultiLibMask = 0;

		if (headerGetEntry(h2, RPMTAG_MULTILIBS, NULL, (void **) &p, NULL))
		    oldmultiLibMask = *p;
		if (headerGetEntry(h, RPMTAG_MULTILIBS, NULL, (void **) &p, NULL))
		    multiLibMask = *p;
		if (oldmultiLibMask && multiLibMask
		    && !(oldmultiLibMask & multiLibMask)) {
		    rpmdep->addedPackages.list[alNum].multiLib = multiLibMask;
		}
	    }
	}
	rpmdbFreeIterator(mi);
    }

    if (headerGetEntry(h, RPMTAG_OBSOLETENAME, NULL, (void **) &obsoletes, &count)) {
	const char **obsoletesEVR;
	int_32 *obsoletesFlags;
	int j;

	headerGetEntry(h, RPMTAG_OBSOLETEVERSION, NULL, (void **) &obsoletesEVR, NULL);
	headerGetEntry(h, RPMTAG_OBSOLETEFLAGS, NULL, (void **) &obsoletesFlags, NULL);

	for (j = 0; j < count; j++) {

	    /* XXX avoid self-obsoleting packages. */
	    if (!strcmp(name, obsoletes[j]))
		continue;

	  { rpmdbMatchIterator mi;
	    Header h2;

	    mi = rpmdbInitIterator(rpmdep->rpmdb, RPMTAG_NAME, obsoletes[j], 0);

	    rpmdbPruneIterator(mi,
		rpmdep->removedPackages, rpmdep->numRemovedPackages, 1);

	    while((h2 = rpmdbNextIterator(mi)) != NULL) {
		/*
		 * Rpm prior to 3.0.3 does not have versioned obsoletes.
		 * If no obsoletes version info is available, match all names.
		 */
		if (obsoletesEVR == NULL ||
		    headerMatchesDepFlags(h2,
			obsoletes[j], obsoletesEVR[j], obsoletesFlags[j]))
		{
		    removePackage(rpmdep, rpmdbGetIteratorOffset(mi), alNum);
		}
	    }
	    rpmdbFreeIterator(mi);
	  }
	}

	if (obsoletesEVR) free(obsoletesEVR);
	free(obsoletes);
    }

    return 0;
}

void rpmtransAvailablePackage(rpmTransactionSet rpmdep, Header h,
	const void * key)
{
    struct availablePackage * al;
    al = alAddPackage(&rpmdep->availablePackages, h, key, NULL, NULL);
}

void rpmtransRemovePackage(rpmTransactionSet rpmdep, int dboffset)
{
    removePackage(rpmdep, dboffset, -1);
}

void rpmtransFree(rpmTransactionSet rpmdep)
{
    struct availableList * addedPackages = &rpmdep->addedPackages;
    struct availableList * availablePackages = &rpmdep->availablePackages;

    alFree(addedPackages);
    alFree(availablePackages);
    if (rpmdep->removedPackages)
	free(rpmdep->removedPackages);
    if (rpmdep->order)
	free(rpmdep->order);
    if (rpmdep->scriptFd)
	rpmdep->scriptFd = fdFree(rpmdep->scriptFd, "rpmtransSetScriptFd (rpmtransFree");
    if (rpmdep->rootDir)
	xfree(rpmdep->rootDir);
    if (rpmdep->currDir)
	xfree(rpmdep->currDir);

    free(rpmdep);
}

void rpmdepFreeConflicts(struct rpmDependencyConflict * conflicts,
			int numConflicts)
{
    int i;

    for (i = 0; i < numConflicts; i++) {
	headerFree(conflicts[i].byHeader);
	free(conflicts[i].byName);
	free(conflicts[i].byVersion);
	free(conflicts[i].byRelease);
	free(conflicts[i].needsName);
	free(conflicts[i].needsVersion);
    }

    free(conflicts);
}

/*@dependent@*/ /*@null@*/ static struct availablePackage *
alFileSatisfiesDepend(struct availableList * al,
	const char * keyType, const char * fileName)
{
    int i;
    const char * dirName;
    const char * baseName;
    struct dirInfo dirNeedle;
    struct dirInfo * dirMatch;

    if (al->numDirs == 0)	/* Solaris 2.6 bsearch sucks down on this. */
	return NULL;

    {	char * chptr = xstrdup(fileName);
	dirName = chptr;
	chptr = strrchr(chptr, '/');
	chptr++;
	*chptr = '\0';
    }

    dirNeedle.dirName = (char *) dirName;
    dirNeedle.dirNameLen = strlen(dirName);
    dirMatch = bsearch(&dirNeedle, al->dirs, al->numDirs,
		       sizeof(dirNeedle), dirInfoCompare);
    xfree(dirName);
    if (!dirMatch) return NULL;

    baseName = strrchr(fileName, '/') + 1;

    /* XXX FIXME: these file lists should be sorted and bsearched */
    for (i = 0; i < dirMatch->numFiles; i++) {
	if (!strcmp(dirMatch->files[i].baseName, baseName)) {

	    /* If a file dependency would be satisfied by a file
	       we are not going to install, skip it. */
	    if (al->list[dirMatch->files[i].pkgNum].multiLib &&
		!isFileMULTILIB(dirMatch->files[i].fileFlags))
		continue;

	    if (keyType)
		rpmMessage(RPMMESS_DEBUG, _("%s: %-45s YES (added files)\n"),
			    keyType, fileName);
	    return al->list + dirMatch->files[i].pkgNum;
	}
    }

    return NULL;
}

/*@dependent@*/ /*@null@*/ static struct availablePackage * alSatisfiesDepend(
	struct availableList * al,
	const char * keyType, const char * keyDepend,
	const char * keyName, const char * keyEVR, int keyFlags)
{
    struct availableIndexEntry needle, * match;
    struct availablePackage * p;
    int i, rc;

    if (*keyName == '/')
	return alFileSatisfiesDepend(al, keyType, keyName);

    if (!al->index.size) return NULL;

    needle.entry = keyName;
    needle.entryLen = strlen(keyName);
    match = bsearch(&needle, al->index.index, al->index.size,
		    sizeof(*al->index.index), indexcmp);

    if (match == NULL) return NULL;

    p = match->package;
    rc = 0;
    switch (match->type) {
    case IET_PROVIDES:
	for (i = 0; i < p->providesCount; i++) {
	    const char *proEVR;
	    int proFlags;

	    /* Filter out provides that came along for the ride. */
	    if (strcmp(p->provides[i], keyName))
		continue;

	    proEVR = (p->providesEVR ? p->providesEVR[i] : NULL);
	    proFlags = (p->provideFlags ? p->provideFlags[i] : 0);
	    rc = rpmRangesOverlap(p->provides[i], proEVR, proFlags,
			keyName, keyEVR, keyFlags);
	    if (rc) break;
	}
	if (keyType && keyDepend && rc)
	    rpmMessage(RPMMESS_DEBUG, _("%s: %-45s YES (added provide)\n"),
			keyType, keyDepend+2);
    	break;
    }

    if (rc)
	return p;

    return NULL;
}

/* 2 == error */
/* 1 == dependency not satisfied */
static int unsatisfiedDepend(rpmTransactionSet rpmdep,
	const char * keyType, const char * keyDepend,
	const char * keyName, const char * keyEVR, int keyFlags,
	/*@out@*/ struct availablePackage ** suggestion)
{
    static int _cacheDependsRC = 1;
    rpmdbMatchIterator mi;
    Header h;
    int rc = 0;	/* assume dependency is satisfied */
    int i;

    if (suggestion) *suggestion = NULL;

    /*
     * Check if dbiOpen/dbiPut failed (e.g. permissions), we can't cache.
     */
    if (_cacheDependsRC) {
	dbiIndex dbi;
	dbi = dbiOpen(rpmdep->rpmdb, RPMDBI_DEPENDS, 0);
	if (dbi == NULL)
	    _cacheDependsRC = 0;
	else {
	    DBC * dbcursor = NULL;
	    size_t keylen = strlen(keyDepend);
	    void * datap = NULL;
	    size_t datalen = 0;
	    int xx;
	    xx = dbiCopen(dbi, &dbcursor, 0);
	    xx = dbiGet(dbi, dbcursor, (void **)&keyDepend, &keylen, &datap, &datalen, 0);
	    if (xx == 0 && datap && datalen == 4) {
		memcpy(&rc, datap, datalen);
		rpmMessage(RPMMESS_DEBUG, _("%s: %-45s %-3s (cached)\n"),
			keyType, keyDepend, (rc ? "NO" : "YES"));
		xx = dbiCclose(dbi, NULL, 0);
		return rc;
	    }
	    xx = dbiCclose(dbi, dbcursor, 0);
	}
    }

  { const char * rcProvidesString;
    const char * start;

    if (!(keyFlags & RPMSENSE_SENSEMASK) &&
	(rcProvidesString = rpmGetVar(RPMVAR_PROVIDES))) {
	i = strlen(keyName);
	while ((start = strstr(rcProvidesString, keyName))) {
	    if (isspace(start[i]) || start[i] == '\0' || start[i] == ',') {
		rpmMessage(RPMMESS_DEBUG, _("%s: %-45s YES (rpmrc provides)\n"),
			keyType, keyDepend+2);
		goto exit;
	    }
	    rcProvidesString = start + 1;
	}
    }
  }

    /*
     * New features in rpm packaging implicitly add versioned dependencies
     * on rpmlib provides. The dependencies look like "rpmlib(YaddaYadda)".
     * Check those dependencies now.
     */
     if (!strncmp(keyName, "rpmlib(", sizeof("rpmlib(")-1)) {
	if (rpmCheckRpmlibProvides(keyName, keyEVR, keyFlags)) {
	    rpmMessage(RPMMESS_DEBUG, _("%s: %-45s YES (rpmlib provides)\n"),
			keyType, keyDepend+2);
	    goto exit;
	}
    }

    if (alSatisfiesDepend(&rpmdep->addedPackages, keyType, keyDepend, keyName, keyEVR, keyFlags)) {
	goto exit;
    }

    /* XXX only the installer does not have the database open here. */
    if (rpmdep->rpmdb != NULL) {
	if (*keyName == '/') {
	    /* keyFlags better be 0! */

	    mi = rpmdbInitIterator(rpmdep->rpmdb, RPMTAG_BASENAMES, keyName, 0);

	    rpmdbPruneIterator(mi,
			rpmdep->removedPackages, rpmdep->numRemovedPackages, 1);

	    while ((h = rpmdbNextIterator(mi)) != NULL) {
		rpmMessage(RPMMESS_DEBUG, _("%s: %-45s YES (db files)\n"),
			keyType, keyDepend+2);
		rpmdbFreeIterator(mi);
		goto exit;
	    }
	    rpmdbFreeIterator(mi);
	}

	mi = rpmdbInitIterator(rpmdep->rpmdb, RPMTAG_PROVIDENAME, keyName, 0);
	rpmdbPruneIterator(mi,
			rpmdep->removedPackages, rpmdep->numRemovedPackages, 1);
	while ((h = rpmdbNextIterator(mi)) != NULL) {
	    if (rangeMatchesDepFlags(h, keyName, keyEVR, keyFlags)) {
		rpmMessage(RPMMESS_DEBUG, _("%s: %-45s YES (db provides)\n"),
			keyType, keyDepend+2);
		rpmdbFreeIterator(mi);
		goto exit;
	    }
	}
	rpmdbFreeIterator(mi);
    }

    if (suggestion)
	*suggestion = alSatisfiesDepend(&rpmdep->availablePackages, NULL, NULL,
				keyName, keyEVR, keyFlags);

    rpmMessage(RPMMESS_DEBUG, _("%s: %-45s NO\n"), keyType, keyDepend+2);
    rc = 1;	/* dependency is unsatisfied */

exit:
    /*
     * If dbiOpen/dbiPut fails (e.g. permissions), we can't cache.
     */
    if (_cacheDependsRC) {
	dbiIndex dbi;
	dbi = dbiOpen(rpmdep->rpmdb, RPMDBI_DEPENDS, 0);
	if (dbi == NULL) {
	    _cacheDependsRC = 0;
	} else {
	    DBC * dbcursor = NULL;
	    int xx;
	    xx = dbiCopen(dbi, &dbcursor, 0);
	    xx = dbiPut(dbi, dbcursor, keyDepend, strlen(keyDepend), &rc, sizeof(rc), 0);
	    if (xx)
		_cacheDependsRC = 0;
#if 0	/* XXX NOISY */
	    else
		rpmMessage(RPMMESS_DEBUG, _("%s: (%s, %s) added to Depends cache.\n"), keyType, keyDepend, (rc ? "NO" : "YES"));
#endif
	    xx = dbiCclose(dbi, dbcursor, 0);
	}
    }
    return rc;
}

static int checkPackageDeps(rpmTransactionSet rpmdep, struct problemsSet * psp,
		Header h, const char * keyName, uint_32 multiLib)
{
    const char * name, * version, * release;
    const char ** requires, ** requiresEVR = NULL;
    const char ** conflicts, ** conflictsEVR = NULL;
    int requiresCount = 0, conflictsCount = 0;
    int type;
    int i, rc;
    int ourrc = 0;
    int_32 * requireFlags = NULL, * conflictFlags = NULL;
    struct availablePackage * suggestion;

    headerNVR(h, &name, &version, &release);

    if (!headerGetEntry(h, RPMTAG_REQUIRENAME, &type, (void **) &requires,
	     &requiresCount)) {
	requiresCount = 0;
    } else {
	headerGetEntry(h, RPMTAG_REQUIREFLAGS, &type, (void **) &requireFlags,
		 &requiresCount);
	headerGetEntry(h, RPMTAG_REQUIREVERSION, &type,
		(void **) &requiresEVR, &requiresCount);
    }

    for (i = 0; i < requiresCount && !ourrc; i++) {
	const char *keyDepend;

	/* Filter out requires that came along for the ride. */
	if (keyName && strcmp(keyName, requires[i]))
	    continue;

	/* If this requirement comes from the core package only, not libraries,
	   then if we're installing the libraries only, don't count it in. */
	if (multiLib && !isDependsMULTILIB(requireFlags[i]))
	    continue;

	keyDepend = printDepend("R", requires[i], requiresEVR[i], requireFlags[i]);

	rc = unsatisfiedDepend(rpmdep, " Requires", keyDepend,
		requires[i], requiresEVR[i], requireFlags[i], &suggestion);

	switch (rc) {
	case 0:		/* requirements are satisfied. */
	    break;
	case 1:		/* requirements are not satisfied. */
	    rpmMessage(RPMMESS_DEBUG, _("package %s-%s-%s require not satisfied: %s\n"),
		    name, version, release, keyDepend+2);

	    if (psp->num == psp->alloced) {
		psp->alloced += 5;
		psp->problems = xrealloc(psp->problems, sizeof(*psp->problems) *
			    psp->alloced);
	    }
	    psp->problems[psp->num].byHeader = headerLink(h);
	    psp->problems[psp->num].byName = xstrdup(name);
	    psp->problems[psp->num].byVersion = xstrdup(version);
	    psp->problems[psp->num].byRelease = xstrdup(release);
	    psp->problems[psp->num].needsName = xstrdup(requires[i]);
	    psp->problems[psp->num].needsVersion = xstrdup(requiresEVR[i]);
	    psp->problems[psp->num].needsFlags = requireFlags[i];
	    psp->problems[psp->num].sense = RPMDEP_SENSE_REQUIRES;

	    if (suggestion)
		psp->problems[psp->num].suggestedPackage = suggestion->key;
	    else
		psp->problems[psp->num].suggestedPackage = NULL;

	    psp->num++;
	    break;
	case 2:		/* something went wrong! */
	default:
	    ourrc = 1;
	    break;
	}
	xfree(keyDepend);
    }

    if (requiresCount) {
	free(requiresEVR);
	free(requires);
    }

    if (!headerGetEntry(h, RPMTAG_CONFLICTNAME, &type, (void **) &conflicts,
	     &conflictsCount)) {
	conflictsCount = 0;
    } else {
	headerGetEntry(h, RPMTAG_CONFLICTFLAGS, &type,
		(void **) &conflictFlags, &conflictsCount);
	headerGetEntry(h, RPMTAG_CONFLICTVERSION, &type,
		(void **) &conflictsEVR, &conflictsCount);
    }

    for (i = 0; i < conflictsCount && !ourrc; i++) {
	const char *keyDepend;

	/* Filter out conflicts that came along for the ride. */
	if (keyName && strcmp(keyName, conflicts[i]))
	    continue;

	/* If this requirement comes from the core package only, not libraries,
	   then if we're installing the libraries only, don't count it in. */
	if (multiLib && !isDependsMULTILIB(conflictFlags[i]))
	    continue;

	keyDepend = printDepend("C", conflicts[i], conflictsEVR[i], conflictFlags[i]);

	rc = unsatisfiedDepend(rpmdep, "Conflicts", keyDepend,
		conflicts[i], conflictsEVR[i], conflictFlags[i], NULL);

	/* 1 == unsatisfied, 0 == satsisfied */
	switch (rc) {
	case 0:		/* conflicts exist. */
	    rpmMessage(RPMMESS_DEBUG, _("package %s conflicts: %s\n"),
		    name, keyDepend+2);

	    if (psp->num == psp->alloced) {
		psp->alloced += 5;
		psp->problems = xrealloc(psp->problems, sizeof(*psp->problems) *
			    psp->alloced);
	    }
	    psp->problems[psp->num].byHeader = headerLink(h);
	    psp->problems[psp->num].byName = xstrdup(name);
	    psp->problems[psp->num].byVersion = xstrdup(version);
	    psp->problems[psp->num].byRelease = xstrdup(release);
	    psp->problems[psp->num].needsName = xstrdup(conflicts[i]);
	    psp->problems[psp->num].needsVersion = xstrdup(conflictsEVR[i]);
	    psp->problems[psp->num].needsFlags = conflictFlags[i];
	    psp->problems[psp->num].sense = RPMDEP_SENSE_CONFLICTS;
	    psp->problems[psp->num].suggestedPackage = NULL;

	    psp->num++;
	    break;
	case 1:		/* conflicts don't exist. */
	    break;
	case 2:		/* something went wrong! */
	default:
	    ourrc = 1;
	    break;
	}
	xfree(keyDepend);
    }

    if (conflictsCount) {
	free(conflictsEVR);
	free(conflicts);
    }

    return ourrc;
}

/* Adding: check name/provides key against each conflict match. */
/* Erasing: check name/provides/filename key against each requiredby match. */
static int checkPackageSet(rpmTransactionSet rpmdep, struct problemsSet * psp,
	const char * key, /*@only@*/ rpmdbMatchIterator mi)
{
    Header h;
    int rc = 0;

    rpmdbPruneIterator(mi,
		rpmdep->removedPackages, rpmdep->numRemovedPackages, 1);
    while ((h = rpmdbNextIterator(mi)) != NULL) {
	if (checkPackageDeps(rpmdep, psp, h, key, 0)) {
	    rc = 1;
	    break;
	}
    }
    rpmdbFreeIterator(mi);

    return rc;
}

/* Erasing: check name/provides/filename key against requiredby matches. */
static int checkDependentPackages(rpmTransactionSet rpmdep,
			struct problemsSet * psp, const char * key)
{
    rpmdbMatchIterator mi;
    mi = rpmdbInitIterator(rpmdep->rpmdb, RPMTAG_REQUIRENAME, key, 0);
    return checkPackageSet(rpmdep, psp, key, mi);
}

/* Adding: check name/provides key against conflicts matches. */
static int checkDependentConflicts(rpmTransactionSet rpmdep,
		struct problemsSet * psp, const char * key)
{
    int rc = 0;

    if (rpmdep->rpmdb) {	/* XXX is this necessary? */
	rpmdbMatchIterator mi;
	mi = rpmdbInitIterator(rpmdep->rpmdb, RPMTAG_CONFLICTNAME, key, 0);
	rc = checkPackageSet(rpmdep, psp, key, mi);
    }

    return rc;
}

/* selection status is one of:

	-1:	selected
	0:	not selected
	> 0:	selection class

   the current selection pass is included as a separate parameter, and is
   incremented when satisfying a prerequisite */

static int addOrderedPack(rpmTransactionSet rpmdep,
			struct availablePackage * package,
			int * ordering, int * orderNumPtr,
			int * selected, int selectionClass,
			int satisfyDepends, const char ** errorStack)
{
    const char ** requires;
    const char ** requiresEVR;
    int_32 * requireFlags;
    int requiresCount;
    int matchNum;
    int packageNum = package - rpmdep->addedPackages.list;
    int i;
    struct availablePackage * match;
    int rc = 0;

    *errorStack++ = package->name;

    if (selected[packageNum] > 0) {
	const char * errorString;
	const char ** stack;
	char * t;

	i = 0;
	stack = errorStack - 1;
	while (*(--stack))
	    i += strlen(*stack) + 1;

	errorString = t = alloca(i + 2);
	while ((++stack) < errorStack) {
	    t = stpcpy(t, *stack);
	    *t++ = ' ';
	}

	rpmError(RPMMESS_PREREQLOOP, _("loop in prerequisite chain: %s"),
		 errorString);

	return 1;
    }

    selected[packageNum] = selectionClass;

    if (headerGetEntry(package->h, RPMTAG_REQUIRENAME, NULL,
			(void **) &requires, &requiresCount)) {
	headerGetEntry(package->h, RPMTAG_REQUIREFLAGS, NULL,
			(void **) &requireFlags, NULL);
	headerGetEntry(package->h, RPMTAG_REQUIREVERSION, NULL,
			(void **) &requiresEVR, NULL);

	for (i = 0; rc == 0 && i < requiresCount; i++) {
	    if (!(satisfyDepends || (requireFlags[i] & RPMSENSE_PREREQ)))
		continue;
	    match = alSatisfiesDepend(&rpmdep->addedPackages, NULL, NULL,
			  requires[i], requiresEVR[i], requireFlags[i]);
	    /* broken dependencies don't concern us */
	    if (!match) continue;

	    /* let this package satisfy its own predependencies */
	    if (match == package) continue;

	    /* the package has already been selected */
	    matchNum = match - rpmdep->addedPackages.list;
	    if(selected[matchNum] == -1 || selected[matchNum] == selectionClass)
		continue;

	    if (requireFlags[i] & RPMSENSE_PREREQ)
		rc = addOrderedPack(rpmdep, match, ordering, orderNumPtr,
			        selected, selectionClass + 1, 1, errorStack);
	    else
		rc = addOrderedPack(rpmdep, match, ordering, orderNumPtr,
			        selected, selectionClass, 1, errorStack);
	}

	free(requires);
	free(requiresEVR);
    }

    /* whew -- add this package */
    if (rc == 0) {
	ordering[(*orderNumPtr)++] = packageNum;
	selected[packageNum] = -1;
    }

    return rc;
}

static int orderListIndexCmp(const void * one, const void * two)
{
    int a = ((const struct orderListIndex *)one)->alIndex;
    int b = ((const struct orderListIndex *)two)->alIndex;
    return (a - b);
}

int rpmdepOrder(rpmTransactionSet rpmdep)
{
    int i, j;
    int * selected;
    int * ordering;
    int orderingCount;
    const char ** errorStack;
    struct transactionElement * newOrder;
    int newOrderCount = 0;
    struct orderListIndex * orderList, * needle, key;

    alMakeIndex(&rpmdep->addedPackages);
    alMakeIndex(&rpmdep->availablePackages);

    selected = alloca(sizeof(*selected) * rpmdep->addedPackages.size);
    memset(selected, 0, sizeof(*selected) * rpmdep->addedPackages.size);

    errorStack = alloca(sizeof(*errorStack) * (rpmdep->addedPackages.size + 1));
    *errorStack++ = NULL;

    ordering = alloca(sizeof(*ordering) * (rpmdep->addedPackages.size + 1));
    orderingCount = 0;

    for (i = 0; i < rpmdep->addedPackages.size; i++) {
	if (!selected[i]) {
	    if (addOrderedPack(rpmdep, rpmdep->addedPackages.list + i,
			       ordering, &orderingCount, selected, 1, 0,
			       errorStack)) {
		return 1;
	    }
	}
    }

    /* The order ends up as installed packages followed by removed packages,
       with removes for upgrades immediately follwing the installation of
       the new package. This would be easier if we could sort the
       addedPackages array, but we store indexes into it in various places. */
    orderList = xmalloc(sizeof(*orderList) * rpmdep->addedPackages.size);
    for (i = 0, j = 0; i < rpmdep->orderCount; i++) {
	if (rpmdep->order[i].type == TR_ADDED) {
	    orderList[j].alIndex = rpmdep->order[i].u.addedIndex;
	    orderList[j].orIndex = i;
	    j++;
	}
    }
    if (j > rpmdep->addedPackages.size) abort();

    qsort(orderList, rpmdep->addedPackages.size, sizeof(*orderList),
	  orderListIndexCmp);

    newOrder = xmalloc(sizeof(*newOrder) * rpmdep->orderCount);
    for (i = 0, newOrderCount = 0; i < orderingCount; i++) {
	key.alIndex = ordering[i];
	needle = bsearch(&key, orderList, rpmdep->addedPackages.size,
			 sizeof(key), orderListIndexCmp);
	/* bsearch should never, ever fail */

	newOrder[newOrderCount++] = rpmdep->order[needle->orIndex];
	for (j = needle->orIndex + 1; j < rpmdep->orderCount; j++) {
	    if (rpmdep->order[j].type == TR_REMOVED &&
		rpmdep->order[j].u.removed.dependsOnIndex == needle->alIndex) {
		newOrder[newOrderCount++] = rpmdep->order[j];
	    } else {
		break;
	    }
	}
    }

    for (i = 0; i < rpmdep->orderCount; i++) {
	if (rpmdep->order[i].type == TR_REMOVED &&
	    rpmdep->order[i].u.removed.dependsOnIndex == -1)  {
	    newOrder[newOrderCount++] = rpmdep->order[i];
	}
    }

    if (newOrderCount != rpmdep->orderCount) abort();

    free(rpmdep->order);
    rpmdep->order = newOrder;
    rpmdep->orderAlloced = rpmdep->orderCount;
    free(orderList);

    return 0;
}

int rpmdepCheck(rpmTransactionSet rpmdep,
		struct rpmDependencyConflict ** conflicts, int * numConflicts)
{
    struct availablePackage * p;
    int i, j;
    int rc;
    rpmdbMatchIterator mi = NULL;
    Header h = NULL;
    struct problemsSet ps;

    ps.alloced = 5;
    ps.num = 0;
    ps.problems = xcalloc(ps.alloced, sizeof(struct rpmDependencyConflict));

    *conflicts = NULL;
    *numConflicts = 0;

    qsort(rpmdep->removedPackages, rpmdep->numRemovedPackages,
	  sizeof(int), intcmp);

    alMakeIndex(&rpmdep->addedPackages);
    alMakeIndex(&rpmdep->availablePackages);

    /* Look at all of the added packages and make sure their dependencies
     * are satisfied.
     */
    p = rpmdep->addedPackages.list;
    for (i = 0; i < rpmdep->addedPackages.size; i++, p++) {

	rc = checkPackageDeps(rpmdep, &ps, p->h, NULL, p->multiLib);
	if (rc)
	    goto exit;

	/* Adding: check name against conflicts matches. */
	rc = checkDependentConflicts(rpmdep, &ps, p->name);
	if (rc)
	    goto exit;

	if (p->providesCount == 0 || p->provides == NULL)
	    continue;

	rc = 0;
	for (j = 0; j < p->providesCount; j++) {
	    /* Adding: check provides key against conflicts matches. */
	    if (checkDependentConflicts(rpmdep, &ps, p->provides[j])) {
		rc = 1;
		break;
	    }
	}
	if (rc)
	    goto exit;
    }

    /* now look at the removed packages and make sure they aren't critical */
    if (rpmdep->numRemovedPackages > 0) {
      mi = rpmdbInitIterator(rpmdep->rpmdb, RPMDBI_PACKAGES, NULL, 0);
      rpmdbAppendIterator(mi, rpmdep->removedPackages,
		rpmdep->numRemovedPackages);
      while ((h = rpmdbNextIterator(mi)) != NULL) {

	{   const char * name;
	    headerNVR(h, &name, NULL, NULL);

	    /* Erasing: check name against requiredby matches. */
	    rc = checkDependentPackages(rpmdep, &ps, name);
	    if (rc)
		goto exit;
	}

	{   const char ** provides;
	    int providesCount;

	    if (headerGetEntry(h, RPMTAG_PROVIDENAME, NULL, (void **) &provides,
				&providesCount)) {
		rc = 0;
		for (j = 0; j < providesCount; j++) {
		    /* Erasing: check provides against requiredby matches. */
		    if (checkDependentPackages(rpmdep, &ps, provides[j])) {
		    rc = 1;
		    break;
		    }
		}
		xfree(provides);
		if (rc)
		    goto exit;
	    }
	}

	{   const char ** baseNames, ** dirNames;
	    int_32 * dirIndexes;
	    int fileCount;
	    char * fileName = NULL;
	    int fileAlloced = 0;
	    int len;

	    if (headerGetEntry(h, RPMTAG_BASENAMES, NULL, 
			   (void **) &baseNames, &fileCount)) {
		headerGetEntry(h, RPMTAG_DIRNAMES, NULL, 
			    (void **) &dirNames, NULL);
		headerGetEntry(h, RPMTAG_DIRINDEXES, NULL, 
			    (void **) &dirIndexes, NULL);
		rc = 0;
		for (j = 0; j < fileCount; j++) {
		    len = strlen(baseNames[j]) + 1 + 
			  strlen(dirNames[dirIndexes[j]]);
		    if (len > fileAlloced) {
			fileAlloced = len * 2;
			fileName = xrealloc(fileName, fileAlloced);
		    }
		    *fileName = '\0';
		    (void) stpcpy( stpcpy(fileName, dirNames[dirIndexes[j]]) , baseNames[j]);
		    /* Erasing: check filename against requiredby matches. */
		    if (checkDependentPackages(rpmdep, &ps, fileName)) {
			rc = 1;
			break;
		    }
		}

		free(fileName);
		free(baseNames);
		free(dirNames);
		if (rc)
		    goto exit;
	    }
	}

      }
      rpmdbFreeIterator(mi);
      mi = NULL;
    }

    if (!ps.num) {
	free(ps.problems);
    } else {
	*conflicts = ps.problems;
	*numConflicts = ps.num;
    }
    ps.problems = NULL;
    rc = 0;

exit:
    if (mi)
	rpmdbFreeIterator(mi);
    if (ps.problems)
	free(ps.problems);
    return rc;
}
