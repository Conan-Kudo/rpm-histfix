/** \ingroup rpmdep
 * \file lib/rpmte.c
 * Routine(s) to handle an "rpmte"  transaction element.
 */
#include "system.h"

#include <rpm/rpmtypes.h>
#include <rpm/rpmlib.h>		/* RPM_MACHTABLE_* */
#include <rpm/rpmds.h>
#include <rpm/rpmfi.h>
#include <rpm/rpmts.h>
#include <rpm/rpmdb.h>

#include "lib/rpmte_internal.h"

#include "debug.h"

int _rpmte_debug = 0;


void rpmteCleanDS(rpmte te)
{
    te->this = rpmdsFree(te->this);
    te->provides = rpmdsFree(te->provides);
    te->requires = rpmdsFree(te->requires);
    te->conflicts = rpmdsFree(te->conflicts);
    te->obsoletes = rpmdsFree(te->obsoletes);
}

/**
 * Destroy transaction element data.
 * @param p		transaction element
 */
static void delTE(rpmte p)
{
    rpmRelocation * r;

    if (p->relocs) {
	for (r = p->relocs; (r->oldPath || r->newPath); r++) {
	    r->oldPath = _free(r->oldPath);
	    r->newPath = _free(r->newPath);
	}
	p->relocs = _free(p->relocs);
    }

    rpmteCleanDS(p);

    p->fi = rpmfiFree(p->fi);

    if (p->fd != NULL)
        p->fd = fdFree(p->fd, RPMDBG_M("delTE"));

    p->os = _free(p->os);
    p->arch = _free(p->arch);
    p->epoch = _free(p->epoch);
    p->name = _free(p->name);
    p->version = _free(p->version);
    p->release = _free(p->release);
    p->NEVR = _free(p->NEVR);
    p->NEVRA = _free(p->NEVRA);

    p->h = headerFree(p->h);
    p->fs = rpmfsFree(p->fs);


    memset(p, 0, sizeof(*p));	/* XXX trash and burn */
    /* FIX: p->{NEVR,name} annotations */
    return;
}

static rpmfi getFI(rpmte p, rpmts ts, Header h)
{
    rpmfiFlags fiflags;
    fiflags = (p->type == TR_ADDED) ? (RPMFI_NOHEADER | RPMFI_FLAGS_INSTALL) :
				      (RPMFI_NOHEADER | RPMFI_FLAGS_ERASE);

    /* relocate stuff in header if necessary */
    if (rpmteType(p) == TR_ADDED && p->relocs) {
	if (!headerIsSource(h) && !headerIsEntry(h, RPMTAG_ORIGBASENAMES)) {
	    rpmRelocateFileList(ts, p, p->relocs, p->nrelocs, h);
	}
    }
    return rpmfiNew(ts, h, RPMTAG_BASENAMES, fiflags);
}

/**
 * Initialize transaction element data from header.
 * @param ts		transaction set
 * @param p		transaction element
 * @param h		header
 * @param key		(TR_ADDED) package retrieval key (e.g. file name)
 * @param relocs	(TR_ADDED) package file relocations
 */
static void addTE(rpmts ts, rpmte p, Header h,
		fnpyKey key,
		rpmRelocation * relocs)
{
    const char *name, *version, *release, *arch, *os;
    struct rpmtd_s td;

    name = version = release = arch = NULL;
    headerNEVRA(h, &name, NULL, &version, &release, &arch);


    p->name = xstrdup(name);
    p->version = xstrdup(version);
    p->release = xstrdup(release);

    if (headerGet(h, RPMTAG_EPOCH, &td, HEADERGET_MINMEM)) {
	p->epoch = rpmtdFormat(&td, RPMTD_FORMAT_STRING, NULL);
    } else {
	p->epoch = NULL;
    }

    p->arch = arch ? xstrdup(arch) : NULL;
    p->archScore = arch ? rpmMachineScore(RPM_MACHTABLE_INSTARCH, arch) : 0;

    headerGet(h, RPMTAG_OS, &td, HEADERGET_MINMEM);
    os = rpmtdGetString(&td);
    p->os = os ? xstrdup(os) : NULL;
    p->osScore = p->os ? rpmMachineScore(RPM_MACHTABLE_INSTOS, p->os) : 0;

    p->isSource = headerIsSource(h);
    
    p->NEVR = headerGetNEVR(h, NULL);
    p->NEVRA = headerGetNEVRA(h, NULL);

    p->nrelocs = 0;
    p->relocs = NULL;
    if (relocs != NULL) {
	rpmRelocation * r;
	int i;

	for (r = relocs; r->oldPath || r->newPath; r++)
	    p->nrelocs++;
	p->relocs = xmalloc((p->nrelocs + 1) * sizeof(*p->relocs));

	for (i = 0, r = relocs; r->oldPath || r->newPath; i++, r++) {
	    p->relocs[i].oldPath = r->oldPath ? xstrdup(r->oldPath) : NULL;
	    p->relocs[i].newPath = r->newPath ? xstrdup(r->newPath) : NULL;
	}
	p->relocs[i].oldPath = NULL;
	p->relocs[i].newPath = NULL;
    }

    p->db_instance = headerGetInstance(h);
    p->key = key;
    p->fd = NULL;

    p->pkgFileSize = 0;

    p->this = rpmdsThis(h, RPMTAG_PROVIDENAME, RPMSENSE_EQUAL);
    p->provides = rpmdsNew(h, RPMTAG_PROVIDENAME, 0);
    p->requires = rpmdsNew(h, RPMTAG_REQUIRENAME, 0);
    p->conflicts = rpmdsNew(h, RPMTAG_CONFLICTNAME, 0);
    p->obsoletes = rpmdsNew(h, RPMTAG_OBSOLETENAME, 0);

    {
	// get number of files by hand as rpmfiNew needs p->fs
	struct rpmtd_s bnames;
	headerGet(h, RPMTAG_BASENAMES, &bnames, HEADERGET_MINMEM);

	p->fs = rpmfsNew(rpmtdCount(&bnames));

	rpmtdFreeData(&bnames);
    }
    p->fi = getFI(p, ts, h);

    /* See if we have pre/posttrans scripts. */
    p->transscripts |= (headerIsEntry(h, RPMTAG_PRETRANS) &&
			 headerIsEntry(h, RPMTAG_PRETRANSPROG)) ?
			RPMTE_HAVE_PRETRANS : 0;
    p->transscripts |= (headerIsEntry(h, RPMTAG_POSTTRANS) &&
			 headerIsEntry(h, RPMTAG_POSTTRANSPROG)) ?
			RPMTE_HAVE_POSTTRANS : 0;

    rpmteColorDS(p, RPMTAG_PROVIDENAME);
    rpmteColorDS(p, RPMTAG_REQUIRENAME);
    return;
}

rpmte rpmteFree(rpmte te)
{
    if (te != NULL) {
	delTE(te);
	te = _free(te);
    }
    return NULL;
}

rpmte rpmteNew(const rpmts ts, Header h,
		rpmElementType type,
		fnpyKey key,
		rpmRelocation * relocs,
		int dboffset,
		rpmalKey pkgKey)
{
    rpmte p = xcalloc(1, sizeof(*p));
    uint32_t *ep; 
    struct rpmtd_s size;

    p->type = type;
    p->pkgKey = pkgKey;
    addTE(ts, p, h, key, relocs);
    switch (type) {
    case TR_ADDED:
	headerGet(h, RPMTAG_SIGSIZE, &size, HEADERGET_DEFAULT);
	if ((ep = rpmtdGetUint32(&size))) {
	    p->pkgFileSize += 96 + 256 + *ep;
	}
	break;
    case TR_REMOVED:
	/* nothing to do */
	break;
    }

    return p;
}

unsigned int rpmteDBInstance(rpmte te) 
{
    return (te != NULL ? te->db_instance : 0);
}

void rpmteSetDBInstance(rpmte te, unsigned int instance) 
{
    if (te != NULL) 
	te->db_instance = instance;
}

Header rpmteHeader(rpmte te)
{
    return (te != NULL && te->h != NULL ? headerLink(te->h) : NULL);
}

Header rpmteSetHeader(rpmte te, Header h)
{
    if (te != NULL)  {
	te->h = headerFree(te->h);
	if (h != NULL)
	    te->h = headerLink(h);
    }
    return NULL;
}

rpmElementType rpmteType(rpmte te)
{
    /* XXX returning negative for unsigned type */
    return (te != NULL ? te->type : -1);
}

const char * rpmteN(rpmte te)
{
    return (te != NULL ? te->name : NULL);
}

const char * rpmteE(rpmte te)
{
    return (te != NULL ? te->epoch : NULL);
}

const char * rpmteV(rpmte te)
{
    return (te != NULL ? te->version : NULL);
}

const char * rpmteR(rpmte te)
{
    return (te != NULL ? te->release : NULL);
}

const char * rpmteA(rpmte te)
{
    return (te != NULL ? te->arch : NULL);
}

const char * rpmteO(rpmte te)
{
    return (te != NULL ? te->os : NULL);
}

int rpmteIsSource(rpmte te)
{
    return (te != NULL ? te->isSource : 0);
}

rpm_color_t rpmteColor(rpmte te)
{
    return (te != NULL ? te->color : 0);
}

rpm_color_t rpmteSetColor(rpmte te, rpm_color_t color)
{
    rpm_color_t ocolor = 0;
    if (te != NULL) {
	ocolor = te->color;
	te->color = color;
    }
    return ocolor;
}

rpm_loff_t rpmtePkgFileSize(rpmte te)
{
    return (te != NULL ? te->pkgFileSize : 0);
}

int rpmteDepth(rpmte te)
{
    return (te != NULL ? te->depth : 0);
}

int rpmteSetDepth(rpmte te, int ndepth)
{
    int odepth = 0;
    if (te != NULL) {
	odepth = te->depth;
	te->depth = ndepth;
    }
    return odepth;
}

int rpmteBreadth(rpmte te)
{
    return (te != NULL ? te->depth : 0);
}

int rpmteSetBreadth(rpmte te, int nbreadth)
{
    int obreadth = 0;
    if (te != NULL) {
	obreadth = te->breadth;
	te->breadth = nbreadth;
    }
    return obreadth;
}

int rpmteNpreds(rpmte te)
{
    return (te != NULL ? te->npreds : 0);
}

int rpmteSetNpreds(rpmte te, int npreds)
{
    int opreds = 0;
    if (te != NULL) {
	opreds = te->npreds;
	te->npreds = npreds;
    }
    return opreds;
}

int rpmteTree(rpmte te)
{
    return (te != NULL ? te->tree : 0);
}

int rpmteSetTree(rpmte te, int ntree)
{
    int otree = 0;
    if (te != NULL) {
	otree = te->tree;
	te->tree = ntree;
    }
    return otree;
}

rpmte rpmteParent(rpmte te)
{
    return (te != NULL ? te->parent : NULL);
}

rpmte rpmteSetParent(rpmte te, rpmte pte)
{
    rpmte opte = NULL;
    if (te != NULL) {
	opte = te->parent;
	te->parent = pte;
    }
    return opte;
}

int rpmteDegree(rpmte te)
{
    return (te != NULL ? te->degree : 0);
}

int rpmteSetDegree(rpmte te, int ndegree)
{
    int odegree = 0;
    if (te != NULL) {
	odegree = te->degree;
	te->degree = ndegree;
    }
    return odegree;
}

tsortInfo rpmteTSI(rpmte te)
{
    return te->tsi;
}

void rpmteFreeTSI(rpmte te)
{
    if (te != NULL && rpmteTSI(te) != NULL) {
	tsortInfo tsi;

	/* Clean up tsort remnants (if any). */
	while ((tsi = rpmteTSI(te)->tsi_next) != NULL) {
	    rpmteTSI(te)->tsi_next = tsi->tsi_next;
	    tsi->tsi_next = NULL;
	    tsi = _free(tsi);
	}
	te->tsi = _free(te->tsi);
    }
    /* FIX: te->tsi is NULL */
    return;
}

void rpmteNewTSI(rpmte te)
{
    if (te != NULL) {
	rpmteFreeTSI(te);
	te->tsi = xcalloc(1, sizeof(*te->tsi));
    }
}

rpmalKey rpmteAddedKey(rpmte te)
{
    return (te != NULL && te->type == TR_ADDED ? te->pkgKey : RPMAL_NOMATCH);
}

rpmalKey rpmteSetAddedKey(rpmte te, rpmalKey npkgKey)
{
    rpmalKey opkgKey = RPMAL_NOMATCH;
    if (te != NULL && te->type == TR_ADDED) {
	opkgKey = te->pkgKey;
	te->pkgKey = npkgKey;
    }
    return opkgKey;
}


rpmalKey rpmteDependsOnKey(rpmte te)
{
    return (te != NULL && te->type == TR_REMOVED ? te->pkgKey : RPMAL_NOMATCH);
}

int rpmteDBOffset(rpmte te)
{
    return rpmteDBInstance(te);
}

const char * rpmteEVR(rpmte te)
{
    return (te != NULL ? te->NEVR + strlen(te->name) + 1 : NULL);
}

const char * rpmteNEVR(rpmte te)
{
    return (te != NULL ? te->NEVR : NULL);
}

const char * rpmteNEVRA(rpmte te)
{
    return (te != NULL ? te->NEVRA : NULL);
}

FD_t rpmteSetFd(rpmte te, FD_t fd)
{
    if (te != NULL)  {
	if (te->fd != NULL)
	    te->fd = fdFree(te->fd, RPMDBG_M("rpmteSetFd"));
	if (fd != NULL)
	    te->fd = fdLink(fd, RPMDBG_M("rpmteSetFd"));
    }
    return NULL;
}

FD_t rpmteFd(rpmte te)
{
    return (te != NULL ? te->fd : NULL);
}

fnpyKey rpmteKey(rpmte te)
{
    return (te != NULL ? te->key : NULL);
}

rpmds rpmteDS(rpmte te, rpmTag tag)
{
    if (te == NULL)
	return NULL;

    if (tag == RPMTAG_NAME)
	return te->this;
    else
    if (tag == RPMTAG_PROVIDENAME)
	return te->provides;
    else
    if (tag == RPMTAG_REQUIRENAME)
	return te->requires;
    else
    if (tag == RPMTAG_CONFLICTNAME)
	return te->conflicts;
    else
    if (tag == RPMTAG_OBSOLETENAME)
	return te->obsoletes;
    else
	return NULL;
}

rpmfi rpmteSetFI(rpmte te, rpmfi fi)
{
    if (te != NULL)  {
	te->fi = rpmfiFree(te->fi);
	if (fi != NULL)
	    te->fi = rpmfiLink(fi, __FUNCTION__);
    }
    return NULL;
}

rpmfi rpmteFI(rpmte te)
{
    if (te == NULL)
	return NULL;

    return te->fi; /* XXX take fi reference here? */
}

void rpmteColorDS(rpmte te, rpmTag tag)
{
    rpmfi fi = rpmteFI(te);
    rpmds ds = rpmteDS(te, tag);
    char deptype = 'R';
    char mydt;
    const uint32_t * ddict;
    rpm_color_t * colors;
    int32_t * refs;
    rpm_color_t val;
    int Count;
    size_t nb;
    unsigned ix;
    int ndx, i;

    if (!(te && (Count = rpmdsCount(ds)) > 0 && rpmfiFC(fi) > 0))
	return;

    switch (tag) {
    default:
	return;
	break;
    case RPMTAG_PROVIDENAME:
	deptype = 'P';
	break;
    case RPMTAG_REQUIRENAME:
	deptype = 'R';
	break;
    }

    colors = xcalloc(Count, sizeof(*colors));
    nb = Count * sizeof(*refs);
    refs = memset(xmalloc(nb), -1, nb);

    /* Calculate dependency color and reference count. */
    fi = rpmfiInit(fi, 0);
    if (fi != NULL)
    while (rpmfiNext(fi) >= 0) {
	val = rpmfiFColor(fi);
	ddict = NULL;
	ndx = rpmfiFDepends(fi, &ddict);
	if (ddict != NULL)
	while (ndx-- > 0) {
	    ix = *ddict++;
	    mydt = ((ix >> 24) & 0xff);
	    if (mydt != deptype)
		continue;
	    ix &= 0x00ffffff;
assert (ix < Count);
	    colors[ix] |= val;
	    refs[ix]++;
	}
    }

    /* Set color/refs values in dependency set. */
    ds = rpmdsInit(ds);
    while ((i = rpmdsNext(ds)) >= 0) {
	val = colors[i];
	te->color |= val;
	(void) rpmdsSetColor(ds, val);
	val = refs[i];
	if (val >= 0)
	    val++;
	(void) rpmdsSetRefs(ds, val);
    }
    free(colors);
    free(refs);
}

int rpmtsiOc(rpmtsi tsi)
{
    return tsi->ocsave;
}

rpmtsi rpmtsiFree(rpmtsi tsi)
{
    /* XXX watchout: a funky recursion segfaults here iff nrefs is wrong. */
    if (tsi)
	tsi->ts = rpmtsFree(tsi->ts);
    return _free(tsi);
}

rpmtsi rpmtsiInit(rpmts ts)
{
    rpmtsi tsi = NULL;

    tsi = xcalloc(1, sizeof(*tsi));
    tsi->ts = rpmtsLink(ts, RPMDBG_M("rpmtsi"));
    tsi->reverse = ((rpmtsFlags(ts) & RPMTRANS_FLAG_REVERSE) ? 1 : 0);
    tsi->oc = (tsi->reverse ? (rpmtsNElements(ts) - 1) : 0);
    tsi->ocsave = tsi->oc;
    return tsi;
}

/**
 * Return next transaction element.
 * @param tsi		transaction element iterator
 * @return		transaction element, NULL on termination
 */
static
rpmte rpmtsiNextElement(rpmtsi tsi)
{
    rpmte te = NULL;
    int oc = -1;

    if (tsi == NULL || tsi->ts == NULL || rpmtsNElements(tsi->ts) <= 0)
	return te;

    if (tsi->reverse) {
	if (tsi->oc >= 0)		oc = tsi->oc--;
    } else {
    	if (tsi->oc < rpmtsNElements(tsi->ts))	oc = tsi->oc++;
    }
    tsi->ocsave = oc;
    if (oc != -1)
	te = rpmtsElement(tsi->ts, oc);
    return te;
}

rpmte rpmtsiNext(rpmtsi tsi, rpmElementType type)
{
    rpmte te;

    while ((te = rpmtsiNextElement(tsi)) != NULL) {
	if (type == 0 || (te->type & type) != 0)
	    break;
    }
    return te;
}

static Header rpmteDBHeader(rpmts ts, unsigned int rec)
{
    Header h = NULL;
    rpmdbMatchIterator mi;

    mi = rpmtsInitIterator(ts, RPMDBI_PACKAGES, &rec, sizeof(rec));
    /* iterator returns weak refs, grab hold of header */
    if ((h = rpmdbNextIterator(mi)))
	h = headerLink(h);
    mi = rpmdbFreeIterator(mi);
    return h;
}

static Header rpmteFDHeader(rpmts ts, rpmte te)
{
    Header h = NULL;
    te->fd = rpmtsNotify(ts, te, RPMCALLBACK_INST_OPEN_FILE, 0, 0);
    if (te->fd != NULL) {
	rpmVSFlags ovsflags;
	rpmRC pkgrc;

	ovsflags = rpmtsSetVSFlags(ts, rpmtsVSFlags(ts) | RPMVSF_NEEDPAYLOAD);
	pkgrc = rpmReadPackageFile(ts, rpmteFd(te), rpmteNEVRA(te), &h);
	rpmtsSetVSFlags(ts, ovsflags);
	switch (pkgrc) {
	default:
	    rpmteClose(te, ts);
	    break;
	case RPMRC_NOTTRUSTED:
	case RPMRC_NOKEY:
	case RPMRC_OK:
	    break;
	}
    }
    return h;
}

int rpmteOpen(rpmte te, rpmts ts, int reload_fi)
{
    Header h = NULL;
    unsigned int instance;
    if (te == NULL || ts == NULL)
	goto exit;

    instance = rpmteDBInstance(te);
    rpmteSetHeader(te, NULL);

    switch (rpmteType(te)) {
    case TR_ADDED:
	h = instance ? rpmteDBHeader(ts, instance) : rpmteFDHeader(ts, te);
	break;
    case TR_REMOVED:
	h = rpmteDBHeader(ts, instance);
    	break;
    }
    if (h != NULL) {
	if (reload_fi) {
	    te->fi = getFI(te, ts, h);
	}
	
	rpmteSetHeader(te, h);
	headerFree(h);
    }

exit:
    return (h != NULL);
}

int rpmteClose(rpmte te, rpmts ts)
{
    if (te == NULL || ts == NULL)
	return 0;

    switch (te->type) {
    case TR_ADDED:
	if (te->fd) {
	    rpmtsNotify(ts, te, RPMCALLBACK_INST_CLOSE_FILE, 0, 0);
	    te->fd = NULL;
	}
	break;
    case TR_REMOVED:
	/* eventually we'll want notifications for erase open too */
	break;
    }
    rpmteSetHeader(te, NULL);
    rpmteSetFI(te, NULL);
    return 1;
}

int rpmteMarkFailed(rpmte te, rpmts ts)
{
    rpmtsi pi = rpmtsiInit(ts);
    int rc = 0;
    rpmte p;
    rpmalKey key = rpmteAddedKey(te);

    te->failed = 1;
    /* XXX we can do a much better here than this... */
    while ((p = rpmtsiNext(pi, TR_REMOVED))) {
	if (rpmteDependsOnKey(p) == key) {
	    p->failed = 1;
	}
    }
    rpmtsiFree(pi);
    return rc;
}

int rpmteFailed(rpmte te)
{
    return (te != NULL) ? te->failed : -1;
}

int rpmteHaveTransScript(rpmte te, rpmTag tag)
{
    int rc = 0;
    if (tag == RPMTAG_PRETRANS) {
	rc = (te->transscripts & RPMTE_HAVE_PRETRANS);
    } else if (tag == RPMTAG_POSTTRANS) {
	rc = (te->transscripts & RPMTE_HAVE_POSTTRANS);
    }
    return rc;
}

rpmfs rpmteGetFileStates(rpmte te) {
    return te->fs;
}

rpmfs rpmfsNew(unsigned int fc) {
    rpmfs fs = xmalloc(sizeof(*fs));
    fs->fc = fc;
    fs->replaced = NULL;
    fs->states = NULL;
    fs->actions = xmalloc(fc * sizeof(*fs->actions));
    memset(fs->actions, FA_UNKNOWN, fc * sizeof(*fs->actions));
    fs->numReplaced = fs->allocatedReplaced = 0;
    return fs;
}

rpmfs rpmfsFree(rpmfs fs) {
    fs->replaced = _free(fs->replaced);
    fs->states = _free(fs->states);
    fs->actions = _free(fs->actions);

    fs = _free(fs);
    return fs;
}

rpm_count_t rpmfsFC(rpmfs fs) {
    return fs->fc;
}

void rpmfsAddReplaced(rpmfs fs, int pkgFileNum, int otherPkg, int otherFileNum)
{
    if (!fs->replaced) {
	fs->replaced = xcalloc(3, sizeof(*fs->replaced));
	fs->allocatedReplaced = 3;
    }
    if (fs->numReplaced>=fs->allocatedReplaced) {
	fs->allocatedReplaced += (fs->allocatedReplaced>>1) + 2;
	fs->replaced = xrealloc(fs->replaced, fs->allocatedReplaced*sizeof(*fs->replaced));
    }
    fs->replaced[fs->numReplaced].pkgFileNum = pkgFileNum;
    fs->replaced[fs->numReplaced].otherPkg = otherPkg;
    fs->replaced[fs->numReplaced].otherFileNum = otherFileNum;

    fs->numReplaced++;
}

sharedFileInfo rpmfsGetReplaced(rpmfs fs)
{
    if (fs && fs->numReplaced)
        return fs->replaced;
    else
        return NULL;
}

sharedFileInfo rpmfsNextReplaced(rpmfs fs , sharedFileInfo replaced)
{
    if (fs && replaced) {
        replaced++;
	if (replaced - fs->replaced < fs->numReplaced)
	    return replaced;
    }
    return NULL;
}

void rpmfsSetState(rpmfs fs, unsigned int ix, rpmfileState state)
{
    assert(ix < fs->fc);
    if (fs->states == NULL) {
	fs->states = xmalloc(sizeof(*fs->states) * fs->fc);
	memset(fs->states, RPMFILE_STATE_MISSING, fs->fc);
    }
    fs->states[ix] = state;
}

rpmfileState rpmfsGetState(rpmfs fs, unsigned int ix)
{
    assert(ix < fs->fc);
    if (fs->states) return fs->states[ix];
    return RPMFILE_STATE_MISSING;
}

rpmfileState * rpmfsGetStates(rpmfs fs)
{
    return fs->states;
}

rpmFileAction rpmfsGetAction(rpmfs fs, unsigned int ix)
{
    rpmFileAction action;
    if (fs->actions != NULL && ix < fs->fc) {
	action = fs->actions[ix];
    } else {
	action = FA_UNKNOWN;
    }
    return action;
}

void rpmfsSetAction(rpmfs fs, unsigned int ix, rpmFileAction action)
{
    if (fs->actions != NULL && ix < fs->fc) {
	fs->actions[ix] = action;
    }
}
