/** \ingroup rpmdep
 * \file lib/depends.c
 */

int _depends_debug = 0;

#include "system.h"

#include <rpmlib.h>

#include "depends.h"
#include "rpmdb.h"
#include "misc.h"
#include "debug.h"

/*@access dbiIndex@*/		/* XXX compared with NULL */
/*@access dbiIndexSet@*/	/* XXX compared with NULL */
/*@access Header@*/		/* XXX compared with NULL */
/*@access rpmdb@*/		/* XXX compared with NULL */
/*@access rpmTransactionSet@*/

static /*@null@*/ void * _free(/*@only@*/ /*@null@*/ const void * this) {
    if (this)   free((void *)this);
    return NULL;
}

int headerNVR(Header h, const char **np, const char **vp, const char **rp)
{
    int type, count;
    if (np) {
	if (!(headerGetEntry(h, RPMTAG_NAME, &type, (void **) np, &count)
	    && type == RPM_STRING_TYPE && count == 1))
		*np = NULL;
    }
    if (vp) {
	if (!(headerGetEntry(h, RPMTAG_VERSION, &type, (void **) vp, &count)
	    && type == RPM_STRING_TYPE && count == 1))
		*vp = NULL;
    }
    if (rp) {
	if (!(headerGetEntry(h, RPMTAG_RELEASE, &type, (void **) rp, &count)
	    && type == RPM_STRING_TYPE && count == 1))
		*rp = NULL;
    }
    return 0;
}

/**
 * Return formatted dependency string.
 * @param depend	type of dependency ("R" == Requires, "C" == Conflcts)
 * @param key		dependency name string
 * @param keyEVR	dependency [epoch:]version[-release] string
 * @param keyFlags	dependency logical range qualifiers
 * @return		formatted dependency (malloc'ed)
 */
static /*@only@*/ char *printDepend(const char * depend, const char * key,
	const char * keyEVR, int keyFlags)	/*@*/
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

/**
 * Destroy available item index.
 * @param al		available list
 */
static void alFreeIndex(struct availableList * al)
	/*@modifies al->index @*/
{
    if (al->index.size) {
	al->index.index = _free(al->index.index);
	al->index.size = 0;
    }
}

/**
 * Initialize available packckages, items, and directories list.
 * @param al		available list
 */
static void alCreate(struct availableList * al)
	/*@modifies *al @*/
{
    al->alloced = al->delta;
    al->size = 0;
    al->list = xcalloc(al->alloced, sizeof(*al->list));

    al->index.index = NULL;
    al->index.size = 0;

    al->numDirs = 0;
    al->dirs = NULL;
}

/**
 * Free available packages, items, and directories members.
 * @param al		available list
 */
static void alFree(struct availableList * al)
{
    HFD_t hfd = headerFreeData;
    struct availablePackage * p;
    rpmRelocation * r;
    int i;

    for (i = 0, p = al->list; i < al->size; i++, p++) {

	{   struct tsortInfo * tsi;
	    while ((tsi = p->tsi.tsi_next) != NULL) {
		p->tsi.tsi_next = tsi->tsi_next;
		tsi->tsi_next = NULL;
		tsi = _free(tsi);
	    }
	}

	p->provides = hfd(p->provides, -1);
	p->providesEVR = hfd(p->providesEVR, -1);
	p->requires = hfd(p->requires, -1);
	p->requiresEVR = hfd(p->requiresEVR, -1);
	p->baseNames = hfd(p->baseNames, -1);
	if (p->h)		headerFree(p->h);

	if (p->relocs) {
	    for (r = p->relocs; (r->oldPath || r->newPath); r++) {
		r->oldPath = _free(r->oldPath);
		r->newPath = _free(r->newPath);
	    }
	    p->relocs = _free(p->relocs);
	}
	if (p->fd)
	    p->fd = fdFree(p->fd, "alAddPackage (alFree)");
    }

    for (i = 0; i < al->numDirs; i++) {
	al->dirs[i].dirName = _free(al->dirs[i].dirName);
	al->dirs[i].files = _free(al->dirs[i].files);
    }

    if (al->numDirs && al->dirs)
	al->dirs = _free(al->dirs);
    if (al->alloced && al->list)
	al->list = _free(al->list);
    alFreeIndex(al);
}

/**
 * Compare two directory info entries by name (qsort/bsearch).
 * @param one		1st directory info
 * @param two		2nd directory info
 * @return		result of comparison
 */
static int dirInfoCompare(const void * one, const void * two)	/*@*/
{
    const struct dirInfo * a = one;
    const struct dirInfo * b = two;
    int lenchk = a->dirNameLen - b->dirNameLen;

    if (lenchk)
	return lenchk;

    /* XXX FIXME: this might do "backward" strcmp for speed */
    return strcmp(a->dirName, b->dirName);
}

/**
 * Add package to available list.
 * @param al		available list
 * @param h		package header
 * @param key		package private data
 * @param fd		package file handle
 * @param relocs	package file relocations
 * @return		available package pointer
 */
static /*@exposed@*/ struct availablePackage * alAddPackage(struct availableList * al,
		Header h, /*@dependent@*/ const void * key,
		FD_t fd, rpmRelocation * relocs)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    int dnt, bnt;
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
	al->alloced += al->delta;
	al->list = xrealloc(al->list, sizeof(*al->list) * al->alloced);
    }

    pkgNum = al->size++;
    p = al->list + pkgNum;
    p->h = headerLink(h);	/* XXX reference held by transaction set */
    memset(&p->tsi, 0, sizeof(p->tsi));
    p->multiLib = 0;	/* MULTILIB */

    headerNVR(p->h, &p->name, &p->version, &p->release);

    /* XXX This should be added always so that packages look alike.
     * XXX However, there is logic in files.c/depends.c that checks for
     * XXX existence (rather than value) that will need to change as well.
     */
    if (hge(p->h, RPMTAG_MULTILIBS, NULL, (void **) &pp, NULL))
	multiLibMask = *pp;

    if (multiLibMask) {
	for (i = 0; i < pkgNum - 1; i++) {
	    if (!strcmp (p->name, al->list[i].name)
		&& hge(al->list[i].h, RPMTAG_MULTILIBS, NULL,
				  (void **) &pp, NULL)
		&& !rpmVersionCompare(p->h, al->list[i].h)
		&& *pp && !(*pp & multiLibMask))
		p->multiLib = multiLibMask;
	}
    }

    if (!hge(h, RPMTAG_EPOCH, NULL, (void **) &p->epoch, NULL))
	p->epoch = NULL;

    if (!hge(h, RPMTAG_PROVIDENAME, NULL, (void **) &p->provides,
	&p->providesCount)) {
	p->providesCount = 0;
	p->provides = NULL;
	p->providesEVR = NULL;
	p->provideFlags = NULL;
    } else {
	if (!hge(h, RPMTAG_PROVIDEVERSION,
			NULL, (void **) &p->providesEVR, NULL))
	    p->providesEVR = NULL;
	if (!hge(h, RPMTAG_PROVIDEFLAGS,
			NULL, (void **) &p->provideFlags, NULL))
	    p->provideFlags = NULL;
    }

    if (!hge(h, RPMTAG_REQUIRENAME, NULL, (void **) &p->requires,
	&p->requiresCount)) {
	p->requiresCount = 0;
	p->requires = NULL;
	p->requiresEVR = NULL;
	p->requireFlags = NULL;
    } else {
	if (!hge(h, RPMTAG_REQUIREVERSION,
			NULL, (void **) &p->requiresEVR, NULL))
	    p->requiresEVR = NULL;
	if (!hge(h, RPMTAG_REQUIREFLAGS,
			NULL, (void **) &p->requireFlags, NULL))
	    p->requireFlags = NULL;
    }

    if (!hge(h, RPMTAG_BASENAMES, &bnt, (void **) &p->baseNames, &p->filesCount))
    {
	p->filesCount = 0;
	p->baseNames = NULL;
    } else {
	hge(h, RPMTAG_DIRNAMES, &dnt, (void **) &dirNames, &numDirs);
	hge(h, RPMTAG_DIRINDEXES, NULL, (void **) &dirIndexes, NULL);
	hge(h, RPMTAG_FILEFLAGS, NULL, (void **) &fileFlags, NULL);

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

	dirNames = hfd(dirNames, dnt);

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

/**
 * Compare two available index entries by name (qsort/bsearch).
 * @param one		1st available index entry
 * @param two		2nd available index entry
 * @return		result of comparison
 */
static int indexcmp(const void * one, const void * two)
{
    const struct availableIndexEntry * a = one;
    const struct availableIndexEntry * b = two;
    int lenchk = a->entryLen - b->entryLen;

    if (lenchk)
	return lenchk;

    return strcmp(a->entry, b->entry);
}

/**
 * Generate index for available list.
 * @param al		available list
 */
static void alMakeIndex(struct availableList * al)
	/*@modifies al->index @*/
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

/**
 * Compare removed package instances (qsort/bsearch).
 * @param a		1st instance address
 * @param b		2nd instance address
 * @return		result of comparison
 */
static int intcmp(const void * a, const void *b)
{
    const int * aptr = a;
    const int * bptr = b;
    int rc = (*aptr - *bptr);
    return rc;
}

/**
 * Split EVR into epoch, version, and release components.
 * @param evr		[epoch:]version[-release] string
 * @retval *ep		pointer to epoch
 * @retval *vp		pointer to version
 * @retval *rp		pointer to release
 */
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
    if (aDepend) free((void *)aDepend);
    if (bDepend) free((void *)bDepend);
    return result;
}

typedef int (*dbrecMatch_t) (Header h, const char *reqName, const char * reqEVR, int reqFlags);

static int rangeMatchesDepFlags (Header h, const char *reqName, const char * reqEVR, int reqFlags)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    int pnt, pvt;
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
    if (!hge(h, RPMTAG_PROVIDEVERSION, &pvt,
		(void **) &providesEVR, &providesCount))
	return 1;

    hge(h, RPMTAG_PROVIDEFLAGS, &type, (void **) &provideFlags, &providesCount);

    if (!hge(h, RPMTAG_PROVIDENAME, &pnt, (void **) &provides, &providesCount))
    {
	providesEVR = hfd(providesEVR, pvt);
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

    provides = hfd(provides, pnt);
    providesEVR = hfd(providesEVR, pvt);

    return result;
}

int headerMatchesDepFlags(Header h,
	const char * reqName, const char * reqEVR, int reqFlags)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
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
    if (hge(h, RPMTAG_EPOCH, NULL, (void **) &epoch, NULL)) {
	sprintf(p, "%d:", *epoch);
	while (*p)
	    p++;
    }
    (void) stpcpy( stpcpy( stpcpy(p, version) , "-") , release);

    return rpmRangesOverlap(name, pkgEVR, pkgFlags, reqName, reqEVR, reqFlags);
}

rpmTransactionSet rpmtransCreateSet(rpmdb rpmdb, const char * rootDir)
{
    rpmTransactionSet ts;
    int rootLen;

    if (!rootDir) rootDir = "";

    ts = xcalloc(1, sizeof(*ts));
    ts->filesystemCount = 0;
    ts->filesystems = NULL;
    ts->di = NULL;
    ts->rpmdb = rpmdb;
    ts->scriptFd = NULL;
    ts->id = 0;
    ts->delta = 5;

    ts->numRemovedPackages = 0;
    ts->allocedRemovedPackages = ts->delta;
    ts->removedPackages = xcalloc(ts->allocedRemovedPackages,
			sizeof(*ts->removedPackages));

    /* This canonicalizes the root */
    rootLen = strlen(rootDir);
    if (!(rootLen && rootDir[rootLen - 1] == '/')) {
	char * t;

	t = alloca(rootLen + 2);
	*t = '\0';
	(void) stpcpy( stpcpy(t, rootDir), "/");
	rootDir = t;
    }

    ts->rootDir = xstrdup(rootDir);
    ts->currDir = NULL;
    ts->chrootDone = 0;

    ts->addedPackages.delta = ts->delta;
    alCreate(&ts->addedPackages);
    ts->availablePackages.delta = ts->delta;
    alCreate(&ts->availablePackages);

    ts->orderAlloced = ts->delta;
    ts->orderCount = 0;
    ts->order = xcalloc(ts->orderAlloced, sizeof(*ts->order));

    return ts;
}

/**
 * Add removed package instance to ordered transaction set.
 * @param ts		transaction set
 * @param dboffset	rpm database instance
 * @param depends	installed package of pair (or -1 on erase)
 */
static void removePackage(rpmTransactionSet ts, int dboffset, int depends)
	/*@modifies ts @*/
{
    if (ts->numRemovedPackages == ts->allocedRemovedPackages) {
	ts->allocedRemovedPackages += ts->delta;
	ts->removedPackages = xrealloc(ts->removedPackages,
		sizeof(int *) * ts->allocedRemovedPackages);
    }

    ts->removedPackages[ts->numRemovedPackages++] = dboffset;

    if (ts->orderCount == ts->orderAlloced) {
	ts->orderAlloced += ts->delta;
	ts->order = xrealloc(ts->order, sizeof(*ts->order) * ts->orderAlloced);
    }

    ts->order[ts->orderCount].type = TR_REMOVED;
    ts->order[ts->orderCount].u.removed.dboffset = dboffset;
    ts->order[ts->orderCount++].u.removed.dependsOnIndex = depends;
}

int rpmtransAddPackage(rpmTransactionSet ts, Header h, FD_t fd,
			const void * key, int upgrade, rpmRelocation * relocs)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    int ont, ovt;
    /* this is an install followed by uninstalls */
    const char * name;
    int count;
    const char ** obsoletes;
    int alNum;

    /*
     * FIXME: handling upgrades like this is *almost* okay. It doesn't
     * check to make sure we're upgrading to a newer version, and it
     * makes it difficult to generate a return code based on the number of
     * packages which failed.
     */
    if (ts->orderCount == ts->orderAlloced) {
	ts->orderAlloced += ts->delta;
	ts->order = xrealloc(ts->order, sizeof(*ts->order) * ts->orderAlloced);
    }
    ts->order[ts->orderCount].type = TR_ADDED;
    alNum = alAddPackage(&ts->addedPackages, h, key, fd, relocs) -
    		ts->addedPackages.list;
    ts->order[ts->orderCount++].u.addedIndex = alNum;

    if (!upgrade || ts->rpmdb == NULL)
	return 0;

    /* XXX binary rpms always have RPMTAG_SOURCERPM, source rpms do not */
    if (headerIsEntry(h, RPMTAG_SOURCEPACKAGE))
	return 0;

    headerNVR(h, &name, NULL, NULL);

    {	rpmdbMatchIterator mi;
	Header h2;

	mi = rpmdbInitIterator(ts->rpmdb, RPMTAG_NAME, name, 0);
	while((h2 = rpmdbNextIterator(mi)) != NULL) {
	    if (rpmVersionCompare(h, h2))
		removePackage(ts, rpmdbGetIteratorOffset(mi), alNum);
	    else {
		uint_32 *p, multiLibMask = 0, oldmultiLibMask = 0;

		if (hge(h2, RPMTAG_MULTILIBS, NULL, (void **) &p, NULL))
		    oldmultiLibMask = *p;
		if (hge(h, RPMTAG_MULTILIBS, NULL, (void **) &p, NULL))
		    multiLibMask = *p;
		if (oldmultiLibMask && multiLibMask
		    && !(oldmultiLibMask & multiLibMask)) {
		    ts->addedPackages.list[alNum].multiLib = multiLibMask;
		}
	    }
	}
	rpmdbFreeIterator(mi);
    }

    if (hge(h, RPMTAG_OBSOLETENAME, &ont, (void **) &obsoletes, &count)) {
	const char **obsoletesEVR;
	int_32 *obsoletesFlags;
	int j;

	hge(h, RPMTAG_OBSOLETEVERSION, &ovt, (void **) &obsoletesEVR, NULL);
	hge(h, RPMTAG_OBSOLETEFLAGS, NULL, (void **) &obsoletesFlags, NULL);

	for (j = 0; j < count; j++) {

	    /* XXX avoid self-obsoleting packages. */
	    if (!strcmp(name, obsoletes[j]))
		continue;

	  { rpmdbMatchIterator mi;
	    Header h2;

	    mi = rpmdbInitIterator(ts->rpmdb, RPMTAG_NAME, obsoletes[j], 0);

	    rpmdbPruneIterator(mi,
		ts->removedPackages, ts->numRemovedPackages, 1);

	    while((h2 = rpmdbNextIterator(mi)) != NULL) {
		/*
		 * Rpm prior to 3.0.3 does not have versioned obsoletes.
		 * If no obsoletes version info is available, match all names.
		 */
		if (obsoletesEVR == NULL ||
		    headerMatchesDepFlags(h2,
			obsoletes[j], obsoletesEVR[j], obsoletesFlags[j]))
		{
		    removePackage(ts, rpmdbGetIteratorOffset(mi), alNum);
		}
	    }
	    rpmdbFreeIterator(mi);
	  }
	}

	obsoletesEVR = hfd(obsoletesEVR, ovt);
	obsoletes = hfd(obsoletes, ont);
    }

    return 0;
}

void rpmtransAvailablePackage(rpmTransactionSet ts, Header h, const void * key)
{
    struct availablePackage * al;
    al = alAddPackage(&ts->availablePackages, h, key, NULL, NULL);
}

void rpmtransRemovePackage(rpmTransactionSet ts, int dboffset)
{
    removePackage(ts, dboffset, -1);
}

void rpmtransFree(rpmTransactionSet ts)
{
    struct availableList * addedPackages = &ts->addedPackages;
    struct availableList * availablePackages = &ts->availablePackages;

    alFree(addedPackages);
    alFree(availablePackages);
    if (ts->di)
	free((void *)ts->di);
    if (ts->removedPackages)
	free(ts->removedPackages);
    if (ts->order)
	free(ts->order);
    if (ts->scriptFd)
	ts->scriptFd = fdFree(ts->scriptFd, "rpmtransSetScriptFd (rpmtransFree");
    if (ts->rootDir)
	free((void *)ts->rootDir);
    if (ts->currDir)
	free((void *)ts->currDir);

    free(ts);
}

void rpmdepFreeConflicts(struct rpmDependencyConflict * conflicts,
			int numConflicts)
{
    int i;

    for (i = 0; i < numConflicts; i++) {
	headerFree(conflicts[i].byHeader);
	free((void *)conflicts[i].byName);
	free((void *)conflicts[i].byVersion);
	free((void *)conflicts[i].byRelease);
	free((void *)conflicts[i].needsName);
	free((void *)conflicts[i].needsVersion);
    }

    free(conflicts);
}

/**
 * Check added package file lists for a file.
 * @param al		available list
 * @param keyType	type of dependency
 * @param fileName	file name to search for
 * @return		available package pointer
 */
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
    free((void *)dirName);
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

/**
 * Check added package file lists for a provide.
 * @param al		available list
 * @param keyType	type of dependency
 * @param keyDepend	dependency string representation
 * @param keyName	dependency name string
 * @param keyEVR	dependency [epoch:]version[-release] string
 * @param keyFlags	dependency logical range qualifiers
 * @return		available package pointer
 */
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

/**
 * Check key for an unsatisfied dependency.
 * @param al		available list
 * @param keyType	type of dependency
 * @param keyDepend	dependency string representation
 * @param keyName	dependency name string
 * @param keyEVR	dependency [epoch:]version[-release] string
 * @param keyFlags	dependency logical range qualifiers
 * @retval suggestion	possible package to resolve dependency
 * @return		0 if satisfied, 1 if not satisfied, 2 if error
 */
static int unsatisfiedDepend(rpmTransactionSet ts,
	const char * keyType, const char * keyDepend,
	const char * keyName, const char * keyEVR, int keyFlags,
	/*@out@*/ struct availablePackage ** suggestion)
{
    static int _cacheDependsRC = 1;
    rpmdbMatchIterator mi;
    Header h;
    int rc = 0;	/* assume dependency is satisfied */

    if (suggestion) *suggestion = NULL;

    /*
     * Check if dbiOpen/dbiPut failed (e.g. permissions), we can't cache.
     */
    if (_cacheDependsRC) {
	dbiIndex dbi;
	dbi = dbiOpen(ts->rpmdb, RPMDBI_DEPENDS, 0);
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

#ifndef	DYING
  { const char * rcProvidesString;
    const char * start;
    int i;

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
#endif

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
	goto unsatisfied;
    }

    if (alSatisfiesDepend(&ts->addedPackages, keyType, keyDepend, keyName, keyEVR, keyFlags)) {
	goto exit;
    }

    /* XXX only the installer does not have the database open here. */
    if (ts->rpmdb != NULL) {
	if (*keyName == '/') {
	    /* keyFlags better be 0! */

	    mi = rpmdbInitIterator(ts->rpmdb, RPMTAG_BASENAMES, keyName, 0);

	    rpmdbPruneIterator(mi,
			ts->removedPackages, ts->numRemovedPackages, 1);

	    while ((h = rpmdbNextIterator(mi)) != NULL) {
		rpmMessage(RPMMESS_DEBUG, _("%s: %-45s YES (db files)\n"),
			keyType, keyDepend+2);
		rpmdbFreeIterator(mi);
		goto exit;
	    }
	    rpmdbFreeIterator(mi);
	}

	mi = rpmdbInitIterator(ts->rpmdb, RPMTAG_PROVIDENAME, keyName, 0);
	rpmdbPruneIterator(mi,
			ts->removedPackages, ts->numRemovedPackages, 1);
	while ((h = rpmdbNextIterator(mi)) != NULL) {
	    if (rangeMatchesDepFlags(h, keyName, keyEVR, keyFlags)) {
		rpmMessage(RPMMESS_DEBUG, _("%s: %-45s YES (db provides)\n"),
			keyType, keyDepend+2);
		rpmdbFreeIterator(mi);
		goto exit;
	    }
	}
	rpmdbFreeIterator(mi);

#ifndef DYING
	mi = rpmdbInitIterator(ts->rpmdb, RPMTAG_NAME, keyName, 0);
	rpmdbPruneIterator(mi,
			ts->removedPackages, ts->numRemovedPackages, 1);
	while ((h = rpmdbNextIterator(mi)) != NULL) {
	    if (rangeMatchesDepFlags(h, keyName, keyEVR, keyFlags)) {
		rpmMessage(RPMMESS_DEBUG, _("%s: %-45s YES (db package)\n"),
			keyType, keyDepend+2);
		rpmdbFreeIterator(mi);
		goto exit;
	    }
	}
	rpmdbFreeIterator(mi);
#endif

    }

    if (suggestion)
	*suggestion = alSatisfiesDepend(&ts->availablePackages, NULL, NULL,
				keyName, keyEVR, keyFlags);

unsatisfied:
    rpmMessage(RPMMESS_DEBUG, _("%s: %-45s NO\n"), keyType, keyDepend+2);
    rc = 1;	/* dependency is unsatisfied */

exit:
    /*
     * If dbiOpen/dbiPut fails (e.g. permissions), we can't cache.
     */
    if (_cacheDependsRC) {
	dbiIndex dbi;
	dbi = dbiOpen(ts->rpmdb, RPMDBI_DEPENDS, 0);
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

static int checkPackageDeps(rpmTransactionSet ts, struct problemsSet * psp,
		Header h, const char * keyName, uint_32 multiLib)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    int rnt, rvt;
    int cnt, cvt;
    const char * name, * version, * release;
    const char ** requires;
    const char ** requiresEVR = NULL;
    int_32 * requireFlags = NULL;
    int requiresCount = 0;
    const char ** conflicts;
    const char ** conflictsEVR = NULL;
    int_32 * conflictFlags = NULL;
    int conflictsCount = 0;
    int type;
    int i, rc;
    int ourrc = 0;
    struct availablePackage * suggestion;

    headerNVR(h, &name, &version, &release);

    if (!hge(h, RPMTAG_REQUIRENAME, &rnt, (void **) &requires, &requiresCount)) {
	requiresCount = 0;
    } else {
	hge(h, RPMTAG_REQUIREFLAGS, &type, (void **) &requireFlags,
		 &requiresCount);
	hge(h, RPMTAG_REQUIREVERSION, &rvt,
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

	rc = unsatisfiedDepend(ts, " Requires", keyDepend,
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
	free((void *)keyDepend);
    }

    if (requiresCount) {
	requiresEVR = hfd(requiresEVR, rvt);
	requires = hfd(requires, rnt);
    }

    if (!hge(h, RPMTAG_CONFLICTNAME, &cnt, (void **)&conflicts, &conflictsCount))
    {
	conflictsCount = 0;
    } else {
	hge(h, RPMTAG_CONFLICTFLAGS, &type,
		(void **) &conflictFlags, &conflictsCount);
	hge(h, RPMTAG_CONFLICTVERSION, &cvt,
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

	rc = unsatisfiedDepend(ts, "Conflicts", keyDepend,
		conflicts[i], conflictsEVR[i], conflictFlags[i], NULL);

	/* 1 == unsatisfied, 0 == satsisfied */
	switch (rc) {
	case 0:		/* conflicts exist. */
	    rpmMessage(RPMMESS_DEBUG, _("package %s conflicts: %s\n"),
		    name, keyDepend+2);

	    if (psp->num == psp->alloced) {
		psp->alloced += 5;
		psp->problems = xrealloc(psp->problems,
					sizeof(*psp->problems) * psp->alloced);
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
	free((void *)keyDepend);
    }

    if (conflictsCount) {
	conflictsEVR = hfd(conflictsEVR, cvt);
	conflicts = hfd(conflicts, cnt);
    }

    return ourrc;
}

/**
 * Adding: check name/provides key against each conflict match,
 * Erasing: check name/provides/filename key against each requiredby match.
 */
static int checkPackageSet(rpmTransactionSet ts, struct problemsSet * psp,
	const char * key, /*@only@*/ rpmdbMatchIterator mi)
{
    Header h;
    int rc = 0;

    rpmdbPruneIterator(mi, ts->removedPackages, ts->numRemovedPackages, 1);
    while ((h = rpmdbNextIterator(mi)) != NULL) {
	if (checkPackageDeps(ts, psp, h, key, 0)) {
	    rc = 1;
	    break;
	}
    }
    rpmdbFreeIterator(mi);

    return rc;
}

/**
 * Erasing: check name/provides/filename key against requiredby matches.
 */
static int checkDependentPackages(rpmTransactionSet ts,
			struct problemsSet * psp, const char * key)
{
    rpmdbMatchIterator mi;
    mi = rpmdbInitIterator(ts->rpmdb, RPMTAG_REQUIRENAME, key, 0);
    return checkPackageSet(ts, psp, key, mi);
}

/**
 * Adding: check name/provides key against conflicts matches.
 */
static int checkDependentConflicts(rpmTransactionSet ts,
		struct problemsSet * psp, const char * key)
{
    int rc = 0;

    if (ts->rpmdb) {	/* XXX is this necessary? */
	rpmdbMatchIterator mi;
	mi = rpmdbInitIterator(ts->rpmdb, RPMTAG_CONFLICTNAME, key, 0);
	rc = checkPackageSet(ts, psp, key, mi);
    }

    return rc;
}

/*
 * XXX Hack to remove known Red Hat dependency loops, will be removed
 * as soon as rpm's legacy permits.
 */
#define	DEPENDENCY_WHITEOUT

#if defined(DEPENDENCY_WHITEOUT)
static struct badDeps_s {
    const char * pname;
    const char * qname;
} badDeps[] = {
    { "libtermcap", "bash" },
    { "modutils", "vixie-cron" },
    { "ypbind", "yp-tools" },
    { "ghostscript-fonts", "ghostscript" },
    /* 7.1 only */
    { "mozilla-psm", "mozilla" },
    { "arts", "kdelibs-sound" },
    /* 7.0 only */
    { "pango-gtkbeta-devel", "pango-gtkbeta" },
    { "XFree86", "Mesa" },
    { "compat-glibc", "db2" },
    { "compat-glibc", "db1" },
    { "pam", "initscripts" },
    { "kernel", "initscripts" },
    { "initscripts", "sysklogd" },
    /* 6.2 */
    { "egcs-c++", "libstdc++" },
    /* 6.1 */
    { "pilot-link-devel", "pilot-link" },
    /* 5.2 */
    { "pam", "pamconfig" },
    { NULL, NULL }
};
    
static int ignoreDep(struct availablePackage * p, struct availablePackage * q)
{
    struct badDeps_s *bdp;

    for (bdp = badDeps; bdp->pname != NULL; bdp++) {
	if (!strcmp(p->name, bdp->pname) && !strcmp(q->name, bdp->qname))
	    return 1;
    }
    return 0;
}
#endif

/**
 * Recursively mark all nodes with their predecessors.
 * @param tsi		successor chain
 * @param q		predecessor
 */
static void markLoop(struct tsortInfo * tsi, struct availablePackage * q)
{
    struct availablePackage * p;

    while (tsi != NULL) {
	p = tsi->tsi_suc;
	tsi = tsi->tsi_next;
	if (p->tsi.tsi_pkg != NULL)
	    continue;
	p->tsi.tsi_pkg = q;
	markLoop(p->tsi.tsi_next, p);
    }
}

static inline /*@observer@*/ const char * const identifyDepend(int_32 f) {
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

/**
 * Find (and eliminate co-requisites) "q <- p" relation in dependency loop.
 * Search all successors of q for instance of p. Format the specific relation,
 * (e.g. p contains "Requires: q"). Unlink and free co-requisite (i.e.
 * pure Requires: dependencies) successor node(s).
 * @param q		sucessor (i.e. package required by p)
 * @param p		predecessor (i.e. package that "Requires: q")
 * @param zap		max. no. of co-requisites to remove (-1 is all)?
 * @retval nzaps	address of no. of relations removed
 * @return		(possibly NULL) formatted "q <- p" releation (malloc'ed)
 */
static /*@owned@*/ /*@null@*/ const char *
zapRelation(struct availablePackage * q, struct availablePackage * p,
	int zap, int * nzaps)
{
    struct tsortInfo * tsi_prev;
    struct tsortInfo * tsi;
    const char *dp = NULL;

    if (q == NULL)
	return dp;
    for (tsi_prev = &q->tsi, tsi = q->tsi.tsi_next;
	 tsi != NULL;
	 tsi_prev = tsi, tsi = tsi->tsi_next)
    {
	int j;

	if (tsi->tsi_suc != p)
	    continue;
	j = tsi->tsi_reqx;
	dp = printDepend( identifyDepend(p->requireFlags[j]),
		p->requires[j], p->requiresEVR[j], p->requireFlags[j]);

	/*
	 * XXX Attempt to unravel a dependency loop by eliminating PreReq's.
	 * This hack "works" for the (relatively) more important autogenerated
	 *	Requires: lib*.so.*
	 * but may cause package %pre/%post scriptlets with, for example,
	 *	PreReq: /bin/sh
	 * to fail.
	 */
	if (zap && !(p->requireFlags[j] & RPMSENSE_PREREQ)) {
	    rpmMessage(RPMMESS_WARNING,
			_("removing %s-%s-%s \"%s\" from tsort relations.\n"),
			p->name, p->version, p->release, dp);
	    p->tsi.tsi_count--;
	    tsi_prev->tsi_next = tsi->tsi_next;
	    tsi->tsi_next = NULL;
	    tsi->tsi_suc = NULL;
	    free(tsi);
	    if (nzaps)
		(*nzaps)++;
	    if (zap)
		zap--;
	}
	break;
    }
    return dp;
}

/**
 * Record next "q <- p" relation (i.e. "p" requires "q").
 * @param ts		transaction set
 * @param p		predecessor (i.e. package that "Requires: q")
 * @param selected	boolean package selected array
 * @param j		relation index
 * @return		0 always
 */
static inline int addRelation( const rpmTransactionSet ts,
		struct availablePackage * p, unsigned char * selected, int j)
{
    struct availablePackage * q;
    struct tsortInfo * tsi;
    int matchNum;

    q = alSatisfiesDepend(&ts->addedPackages, NULL, NULL,
		p->requires[j], p->requiresEVR[j], p->requireFlags[j]);

    /* Ordering depends only on added package relations. */
    if (q == NULL)
	return 0;

    /* Avoid rpmlib feature dependencies. */
    if (!strncmp(p->requires[j], "rpmlib(", sizeof("rpmlib(")-1))
	return 0;

#if defined(DEPENDENCY_WHITEOUT)
    /* Avoid certain dependency relations. */
    if (ignoreDep(p, q))
	return 0;
#endif

    /* Avoid redundant relations. */
    /* XXX FIXME: add control bit. */
    matchNum = q - ts->addedPackages.list;
    if (selected[matchNum])
	return 0;
    selected[matchNum] = 1;

    /* T3. Record next "q <- p" relation (i.e. "p" requires "q"). */
    p->tsi.tsi_count++;			/* bump p predecessor count */
    tsi = xmalloc(sizeof(*tsi));
    tsi->tsi_suc = p;
    tsi->tsi_reqx = j;
    tsi->tsi_next = q->tsi.tsi_next;
    q->tsi.tsi_next = tsi;
    q->tsi.tsi_qcnt++;			/* bump q successor count */
    return 0;
}

/**
 * Compare ordered list entries by index (qsort/bsearch).
 * @param a		1st ordered list entry
 * @param b		2nd ordered list entry
 * @return		result of comparison
 */
static int orderListIndexCmp(const void * one, const void * two)
{
    int a = ((const struct orderListIndex *)one)->alIndex;
    int b = ((const struct orderListIndex *)two)->alIndex;
    return (a - b);
}

/**
 * Add element to list sorting by initial successor count.
 * @param p		new element
 * @retval qp		address of first element
 * @retval rp		address of last element
 */
static void addQ(/*@kept@*/ struct availablePackage * p,
	/*@out@*/ struct availablePackage ** qp,
	/*@out@*/ struct availablePackage ** rp)
{
    struct availablePackage *q, *qprev;

    if ((*rp) == NULL) {	/* 1st element */
	(*rp) = (*qp) = p;
	return;
    }
    for (qprev = NULL, q = (*qp); q != NULL; qprev = q, q = q->tsi.tsi_suc) {
	if (q->tsi.tsi_qcnt <= p->tsi.tsi_qcnt)
	    break;
    }
    if (qprev == NULL) {	/* insert at beginning of list */
	p->tsi.tsi_suc = q;
	(*qp) = p;		/* new head */
    } else if (q == NULL) {	/* insert at end of list */
	qprev->tsi.tsi_suc = p;
	(*rp) = p;		/* new tail */
    } else {			/* insert between qprev and q */
	p->tsi.tsi_suc = q;
	qprev->tsi.tsi_suc = p;
    }
}

int rpmdepOrder(rpmTransactionSet ts)
{
    int npkgs = ts->addedPackages.size;
    struct availablePackage * p;
    struct availablePackage * q;
    struct availablePackage * r;
    struct tsortInfo * tsi;
    struct tsortInfo * tsi_next;
    int * ordering = alloca(sizeof(*ordering) * (npkgs + 1));
    int orderingCount = 0;
    unsigned char * selected = alloca(sizeof(*selected) * (npkgs + 1));
    int loopcheck;
    struct transactionElement * newOrder;
    int newOrderCount = 0;
    struct orderListIndex * orderList;
    int nrescans = 10;
    int _printed = 0;
    int qlen;
    int i, j;

    alMakeIndex(&ts->addedPackages);
    alMakeIndex(&ts->availablePackages);

    /* T1. Initialize. */
    loopcheck = npkgs;

    /* Record all relations. */
    rpmMessage(RPMMESS_DEBUG, _("========== recording tsort relations\n"));
    for (i = 0, p = ts->addedPackages.list; i < npkgs; i++, p++) {
	int matchNum;

	if (p->requiresCount <= 0)
	    continue;

	memset(selected, 0, sizeof(*selected) * npkgs);

	/* Avoid narcisstic relations. */
	matchNum = p - ts->addedPackages.list;
	selected[matchNum] = 1;

	/* T2. Next "q <- p" relation. */

	/* First, do pre-requisites. */
	for (j = 0; j < p->requiresCount; j++) {

	    /* Skip if not %pre/%post requires or legacy prereq. */

	    if (isErasePreReq(p->requireFlags[j]) ||
		!( isInstallPreReq(p->requireFlags[j]) ||
		   isLegacyPreReq(p->requireFlags[j]) ))
		continue;

	    /* T3. Record next "q <- p" relation (i.e. "p" requires "q"). */
	    (void) addRelation(ts, p, selected, j);

	}

	/* Then do co-requisites. */
	for (j = 0; j < p->requiresCount; j++) {

	    /* Skip if %pre/%post requires or legacy prereq. */

	    if (isErasePreReq(p->requireFlags[j]) ||
		 ( isInstallPreReq(p->requireFlags[j]) ||
		   isLegacyPreReq(p->requireFlags[j]) ))
		continue;

	    /* T3. Record next "q <- p" relation (i.e. "p" requires "q"). */
	    (void) addRelation(ts, p, selected, j);

	}
    }

    /* T4. Scan for zeroes. */
    rpmMessage(RPMMESS_DEBUG, _("========== tsorting packages\n"));

rescan:
    q = r = NULL;
    qlen = 0;
    for (i = 0, p = ts->addedPackages.list; i < npkgs; i++, p++) {

	/* Prefer packages in presentation order. */
	p->tsi.tsi_qcnt = (npkgs - i);

	if (p->tsi.tsi_count != 0)
	    continue;
	p->tsi.tsi_suc = NULL;
	addQ(p, &q, &r);
	qlen++;
    }

    /* T5. Output front of queue (T7. Remove from queue.) */
    for (; q != NULL; q = q->tsi.tsi_suc) {

	rpmMessage(RPMMESS_DEBUG, "%5d (%d,%d) %s-%s-%s\n", orderingCount,
			qlen, q->tsi.tsi_qcnt,
			q->name, q->version, q->release);
	ordering[orderingCount++] = q - ts->addedPackages.list;
	qlen--;
	loopcheck--;

	/* T6. Erase relations. */
	tsi_next = q->tsi.tsi_next;
	q->tsi.tsi_next = NULL;
	while ((tsi = tsi_next) != NULL) {
	    tsi_next = tsi->tsi_next;
	    tsi->tsi_next = NULL;
	    p = tsi->tsi_suc;
	    if ((--p->tsi.tsi_count) <= 0) {
		/* XXX FIXME: add control bit. */
		p->tsi.tsi_suc = NULL;
		addQ(p, &q->tsi.tsi_suc, &r);
		qlen++;
	    }
	    free(tsi);
	}
	if (!_printed && loopcheck == qlen && q->tsi.tsi_suc != NULL) {
	    _printed++;
	    rpmMessage(RPMMESS_DEBUG,
		_("========== successors only (presentation order)\n"));
	}
    }

    /* T8. End of process. Check for loops. */
    if (loopcheck != 0) {
	int nzaps;

	/* T9. Initialize predecessor chain. */
	nzaps = 0;
	for (i = 0, q = ts->addedPackages.list; i < npkgs; i++, q++) {
	    q->tsi.tsi_pkg = NULL;
	    q->tsi.tsi_reqx = 0;
	    /* Mark packages already sorted. */
	    if (q->tsi.tsi_count == 0)
		q->tsi.tsi_count = -1;
	}

	/* T10. Mark all packages with their predecessors. */
	for (i = 0, q = ts->addedPackages.list; i < npkgs; i++, q++) {
	    if ((tsi = q->tsi.tsi_next) == NULL)
		continue;
	    q->tsi.tsi_next = NULL;
	    markLoop(tsi, q);
	    q->tsi.tsi_next = tsi;
	}

	/* T11. Print all dependency loops. */
	for (i = 0, r = ts->addedPackages.list; i < npkgs; i++, r++) {
	    int printed;

	    printed = 0;

	    /* T12. Mark predecessor chain, looking for start of loop. */
	    for (q = r->tsi.tsi_pkg; q != NULL; q = q->tsi.tsi_pkg) {
		if (q->tsi.tsi_reqx)
		    break;
		q->tsi.tsi_reqx = 1;
	    }

	    /* T13. Print predecessor chain from start of loop. */
	    while ((p = q) != NULL && (q = p->tsi.tsi_pkg) != NULL) {
		const char * dp;
		char buf[4096];

		/* Unchain predecessor loop. */
		p->tsi.tsi_pkg = NULL;

		if (!printed) {
		    rpmMessage(RPMMESS_WARNING, _("LOOP:\n"));
		    printed = 1;
		}

		/* Find (and destroy if co-requisite) "q <- p" relation. */
		dp = zapRelation(q, p, 1, &nzaps);

		/* Print next member of loop. */
		sprintf(buf, "%s-%s-%s", p->name, p->version, p->release);
		rpmMessage(RPMMESS_WARNING, "    %-40s %s\n", buf, dp);

		if (dp) {
		    free((void *)dp);
		    dp = NULL;
		}
	    }

	    /* Walk (and erase) linear part of predecessor chain as well. */
	    for (p = r, q = r->tsi.tsi_pkg;
		 q != NULL;
		 p = q, q = q->tsi.tsi_pkg)
	    {
		/* Unchain linear part of predecessor loop. */
		p->tsi.tsi_pkg = NULL;
		p->tsi.tsi_reqx = 0;
	    }
	}

	/* If a relation was eliminated, then continue sorting. */
	/* XXX FIXME: add control bit. */
	if (nzaps && nrescans-- > 0) {
	    rpmMessage(RPMMESS_DEBUG, _("========== continuing tsort ...\n"));
	    goto rescan;
	}
	return 1;
    }

    /*
     * The order ends up as installed packages followed by removed packages,
     * with removes for upgrades immediately following the installation of
     * the new package. This would be easier if we could sort the
     * addedPackages array, but we store indexes into it in various places.
     */
    orderList = xmalloc(sizeof(*orderList) * npkgs);
    for (i = 0, j = 0; i < ts->orderCount; i++) {
	if (ts->order[i].type == TR_ADDED) {
	    orderList[j].alIndex = ts->order[i].u.addedIndex;
	    orderList[j].orIndex = i;
	    j++;
	}
    }
    assert(j <= npkgs);

    qsort(orderList, npkgs, sizeof(*orderList), orderListIndexCmp);

    newOrder = xmalloc(sizeof(*newOrder) * ts->orderCount);
    for (i = 0, newOrderCount = 0; i < orderingCount; i++) {
	struct orderListIndex * needle, key;

	key.alIndex = ordering[i];
	needle = bsearch(&key, orderList, npkgs, sizeof(key),orderListIndexCmp);
	/* bsearch should never, ever fail */

	newOrder[newOrderCount++] = ts->order[needle->orIndex];
	for (j = needle->orIndex + 1; j < ts->orderCount; j++) {
	    if (ts->order[j].type == TR_REMOVED &&
		ts->order[j].u.removed.dependsOnIndex == needle->alIndex) {
		newOrder[newOrderCount++] = ts->order[j];
	    } else {
		break;
	    }
	}
    }

    for (i = 0; i < ts->orderCount; i++) {
	if (ts->order[i].type == TR_REMOVED &&
	    ts->order[i].u.removed.dependsOnIndex == -1)  {
	    newOrder[newOrderCount++] = ts->order[i];
	}
    }
    assert(newOrderCount == ts->orderCount);

    free(ts->order);
    ts->order = newOrder;
    ts->orderAlloced = ts->orderCount;
    free(orderList);

    return 0;
}

int rpmdepCheck(rpmTransactionSet ts,
		struct rpmDependencyConflict ** conflicts, int * numConflicts)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    int npkgs = ts->addedPackages.size;
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

    qsort(ts->removedPackages, ts->numRemovedPackages, sizeof(int), intcmp);

    alMakeIndex(&ts->addedPackages);
    alMakeIndex(&ts->availablePackages);

    /*
     * Look at all of the added packages and make sure their dependencies
     * are satisfied.
     */
    for (i = 0, p = ts->addedPackages.list; i < npkgs; i++, p++)
    {

	rc = checkPackageDeps(ts, &ps, p->h, NULL, p->multiLib);
	if (rc)
	    goto exit;

	/* Adding: check name against conflicts matches. */
	rc = checkDependentConflicts(ts, &ps, p->name);
	if (rc)
	    goto exit;

	if (p->providesCount == 0 || p->provides == NULL)
	    continue;

	rc = 0;
	for (j = 0; j < p->providesCount; j++) {
	    /* Adding: check provides key against conflicts matches. */
	    if (!checkDependentConflicts(ts, &ps, p->provides[j]))
		continue;
	    rc = 1;
	    break;
	}
	if (rc)
	    goto exit;
    }

    /*
     * Look at the removed packages and make sure they aren't critical.
     */
    if (ts->numRemovedPackages > 0) {
      mi = rpmdbInitIterator(ts->rpmdb, RPMDBI_PACKAGES, NULL, 0);
      rpmdbAppendIterator(mi, ts->removedPackages, ts->numRemovedPackages);
      while ((h = rpmdbNextIterator(mi)) != NULL) {

	{   const char * name;
	    headerNVR(h, &name, NULL, NULL);

	    /* Erasing: check name against requiredby matches. */
	    rc = checkDependentPackages(ts, &ps, name);
	    if (rc)
		goto exit;
	}

	{   const char ** provides;
	    int providesCount;
	    int pnt;

	    if (hge(h, RPMTAG_PROVIDENAME, &pnt, (void **) &provides,
				&providesCount))
	    {
		rc = 0;
		for (j = 0; j < providesCount; j++) {
		    /* Erasing: check provides against requiredby matches. */
		    if (!checkDependentPackages(ts, &ps, provides[j]))
			continue;
		    rc = 1;
		    break;
		}
		provides = hfd(provides, pnt);
		if (rc)
		    goto exit;
	    }
	}

	{   const char ** baseNames, ** dirNames;
	    int_32 * dirIndexes, dnt, bnt;
	    int fileCount;
	    char * fileName = NULL;
	    int fileAlloced = 0;
	    int len;

	    if (hge(h, RPMTAG_BASENAMES, &bnt, (void **) &baseNames, &fileCount))
	    {
		hge(h, RPMTAG_DIRNAMES, &dnt, (void **) &dirNames, NULL);
		hge(h, RPMTAG_DIRINDEXES, NULL, (void **) &dirIndexes, NULL);
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
		    if (!checkDependentPackages(ts, &ps, fileName))
			continue;
		    rc = 1;
		    break;
		}

		free(fileName);
		baseNames = hfd(baseNames, bnt);
		dirNames = hfd(dirNames, dnt);
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
