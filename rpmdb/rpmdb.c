/** \ingroup rpmdb dbi
 * \file rpmdb/rpmdb.c
 */

#include "system.h"

static int _debug = 0;
#define	INLINE

#include <sys/file.h>
#include <signal.h>
#include <sys/signal.h>

#include <regex.h>

#include <rpmcli.h>

#include "rpmdb.h"
#include "fprint.h"
#include "misc.h"
#include "debug.h"

/*@access dbiIndexSet@*/
/*@access dbiIndexItem@*/
/*@access Header@*/		/* XXX compared with NULL */
/*@access rpmdbMatchIterator@*/

/*@-redecl@*/
extern int _noDirTokens;
/*@=redecl@*/
static int _rebuildinprogress = 0;
static int _db_filter_dups = 0;

#define	_DBI_FLAGS	0
#define	_DBI_PERMS	0644
#define	_DBI_MAJOR	-1

static int dbiTagsMax = 0;
/*@only@*/ /*@null@*/ static int *dbiTags = NULL;

/**
 * Return dbi index used for rpm tag.
 * @param rpmtag	rpm header tag
 * @return		dbi index, -1 on error
 */
static int dbiTagToDbix(int rpmtag)
	/*@*/
{
    int dbix;

    if (dbiTags != NULL)
    for (dbix = 0; dbix < dbiTagsMax; dbix++) {
	if (rpmtag == dbiTags[dbix])
	    return dbix;
    }
    return -1;
}

/**
 * Initialize database (index, tag) tuple from configuration.
 */
static void dbiTagsInit(void)
	/*@modifies dbiTags, dbiTagsMax @*/
{
/*@observer@*/ static const char * const _dbiTagStr_default =
	"Packages:Name:Basenames:Group:Requirename:Providename:Conflictname:Triggername:Dirnames:Requireversion:Provideversion:Installtid:Removetid";
    char * dbiTagStr = NULL;
    char * o, * oe;
    int rpmtag;

    /*@-nullpass@*/
    dbiTagStr = rpmExpand("%{_dbi_tags}", NULL);
    /*@=nullpass@*/
    if (!(dbiTagStr && *dbiTagStr && *dbiTagStr != '%')) {
	dbiTagStr = _free(dbiTagStr);
	dbiTagStr = xstrdup(_dbiTagStr_default);
    }

    /* Discard previous values. */
    dbiTags = _free(dbiTags);
    dbiTagsMax = 0;

    /* Always allocate package index */
    dbiTags = xcalloc(1, sizeof(*dbiTags));
    dbiTags[dbiTagsMax++] = RPMDBI_PACKAGES;

    for (o = dbiTagStr; o && *o; o = oe) {
	while (*o && xisspace(*o))
	    o++;
	if (*o == '\0')
	    break;
	for (oe = o; oe && *oe; oe++) {
	    if (xisspace(*oe))
		/*@innerbreak@*/ break;
	    if (oe[0] == ':' && !(oe[1] == '/' && oe[2] == '/'))
		/*@innerbreak@*/ break;
	}
	if (oe && *oe)
	    *oe++ = '\0';
	rpmtag = tagValue(o);
	if (rpmtag < 0) {

	    fprintf(stderr, _("dbiTagsInit: unrecognized tag name: \"%s\" ignored\n"), o);
	    continue;
	}
	if (dbiTagToDbix(rpmtag) >= 0)
	    continue;

	dbiTags = xrealloc(dbiTags, (dbiTagsMax + 1) * sizeof(*dbiTags)); /* XXX memory leak */
	dbiTags[dbiTagsMax++] = rpmtag;
    }

    dbiTagStr = _free(dbiTagStr);
}

/*@-redecl@*/
#if USE_DB1
extern struct _dbiVec db1vec;
#define	DB1vec		&db1vec
#else
#define	DB1vec		NULL
#endif

#if USE_DB2
extern struct _dbiVec db2vec;
#define	DB2vec		&db2vec
#else
#define	DB2vec		NULL
#endif

#if USE_DB3
extern struct _dbiVec db3vec;
#define	DB3vec		&db3vec
#else
#define	DB3vec		NULL
#endif
/*@=redecl@*/

/*@-nullassign@*/
static struct _dbiVec *mydbvecs[] = {
    DB1vec, DB1vec, DB2vec, DB3vec, NULL
};
/*@=nullassign@*/

INLINE int dbiSync(dbiIndex dbi, unsigned int flags)
{
if (_debug < 0 || dbi->dbi_debug)
fprintf(stderr, "    Sync %s\n", tagName(dbi->dbi_rpmtag));
    return (*dbi->dbi_vec->sync) (dbi, flags);
}

INLINE int dbiByteSwapped(dbiIndex dbi)
{
    return (*dbi->dbi_vec->byteswapped) (dbi);
}

INLINE int dbiCopen(dbiIndex dbi, /*@out@*/ DBC ** dbcp, unsigned int flags)
{
if (_debug < 0 || dbi->dbi_debug)
fprintf(stderr, "+++ RMW %s %s\n", tagName(dbi->dbi_rpmtag), ((flags & DBI_WRITECURSOR) ? "WRITECURSOR" : ""));
    return (*dbi->dbi_vec->copen) (dbi, dbcp, flags);
}

INLINE int dbiCclose(dbiIndex dbi, /*@only@*/DBC * dbcursor, unsigned int flags)
{
if (_debug < 0 || dbi->dbi_debug)
fprintf(stderr, "--- RMW %s\n", tagName(dbi->dbi_rpmtag));
    return (*dbi->dbi_vec->cclose) (dbi, dbcursor, flags);
}

static int printable(const void * ptr, size_t len)	/*@*/
{
    const char * s = ptr;
    int i;
    for (i = 0; i < len; i++, s++)
	if (!(*s >= ' ' && *s <= '~')) return 0;
    return 1;
}

INLINE int dbiDel(dbiIndex dbi, DBC * dbcursor,
	const void * keyp, size_t keylen, unsigned int flags)
{
    int NULkey;
    int rc;

    /* Make sure that keylen is correct for "" lookup. */
    NULkey = (keyp && *((char *)keyp) == '\0' && keylen == 0);
    if (NULkey) keylen++;
    rc = (*dbi->dbi_vec->cdel) (dbi, dbcursor, keyp, keylen, flags);
    if (NULkey) keylen--;

if (_debug < 0 || dbi->dbi_debug)
fprintf(stderr, "    Del %s key (%p,%ld) %s rc %d\n", tagName(dbi->dbi_rpmtag), keyp, (long)keylen, (dbi->dbi_rpmtag != RPMDBI_PACKAGES ? (char *)keyp : ""), rc);

    return rc;
}

INLINE int dbiGet(dbiIndex dbi, DBC * dbcursor, void ** keypp, size_t * keylenp,
	void ** datapp, size_t * datalenp, unsigned int flags)
{
    int NULkey;
    int rc;

    /* Make sure that keylen is correct for "" lookup. */
    NULkey = (keypp && *keypp && *((char *)(*keypp)) == '\0');
    NULkey = (keylenp && *keylenp == 0 && NULkey);
    if (keylenp && NULkey) (*keylenp)++;
    rc = (*dbi->dbi_vec->cget) (dbi, dbcursor,
		keypp, keylenp, datapp, datalenp, flags);
    if (keylenp && NULkey) (*keylenp)--;

/*@-nullderef -nullpass@*/
if (_debug < 0 || dbi->dbi_debug) {
 int dataval = 0xdeadbeef;
 const char * kvp;
 char keyval[64];
 keyval[0] = '\0';
 if (keypp && *keypp && keylenp) {
  if (*keylenp <= sizeof(int) && !printable(*keypp, *keylenp)) {
    int keyint = 0;
    memcpy(&keyint, *keypp, sizeof(keyint));
    sprintf(keyval, "#%d", keyint);
    kvp = keyval;
  } else {
    kvp = *keypp;
  }
 } else
   kvp = keyval;
 if (rc == 0 && datapp && *datapp && datalenp && *datalenp >= sizeof(dataval)) {
    memcpy(&dataval, *datapp, sizeof(dataval));
 }
 fprintf(stderr, "    Get %s key (%p,%ld) data (%p,%ld) \"%s\" %x rc %d\n",
    tagName(dbi->dbi_rpmtag), *keypp, (long)*keylenp, *datapp, (long)*datalenp,
    kvp, (unsigned)dataval, rc);
}
/*@=nullderef =nullpass@*/
    return rc;
}

INLINE int dbiPut(dbiIndex dbi, DBC * dbcursor,
	const void * keyp, size_t keylen,
	const void * datap, size_t datalen, unsigned int flags)
{
    int NULkey;
    int rc;

    /* XXX make sure that keylen is correct for "" lookup */
    NULkey = (keyp && *((char *)keyp) == '\0' && keylen == 0);
    if (NULkey) keylen++;
    rc = (*dbi->dbi_vec->cput) (dbi, dbcursor, keyp, keylen, datap, datalen, flags);
    if (NULkey) keylen--;

/*@-nullderef -nullpass@*/
if (_debug < 0 || dbi->dbi_debug) {
 int dataval = 0xdeadbeef;
 const char * kvp;
 char keyval[64];
 keyval[0] = '\0';
 if (keyp) {
  if (keylen == sizeof(int) && !printable(keyp, keylen)) {
    int keyint = 0;
    memcpy(&keyint, keyp, sizeof(keyint));
    sprintf(keyval, "#%d", keyint);
    kvp = keyval;
  } else {
    kvp = keyp;
  }
 } else
   kvp = keyval;
 if (rc == 0 && datap && datalen >= sizeof(dataval)) {
    memcpy(&dataval, datap, sizeof(dataval));
 }
 fprintf(stderr, "    Put %s key (%p,%ld) data (%p,%ld) \"%s\" %x rc %d\n", tagName(dbi->dbi_rpmtag), keyp, (long)keylen, (datap ? datap : NULL), (long)datalen, kvp, (unsigned)dataval, rc);
}
/*@=nullderef =nullpass@*/

    return rc;
}

INLINE int dbiCount(dbiIndex dbi, DBC * dbcursor,
	unsigned int * countp, unsigned int flags)
{
    int rc = (*dbi->dbi_vec->ccount) (dbi, dbcursor, countp, flags);

if (rc == 0 && countp && *countp > 1)
fprintf(stderr, "    Count %s: %u rc %d\n", tagName(dbi->dbi_rpmtag), *countp, rc);

    return rc;
}

INLINE int dbiVerify(dbiIndex dbi, unsigned int flags)
{
    int dbi_debug = dbi->dbi_debug;
    int dbi_rpmtag = dbi->dbi_rpmtag;
    int rc;

    dbi->dbi_verify_on_close = 1;
    rc = (*dbi->dbi_vec->close) (dbi, flags);

if (_debug < 0 || dbi_debug)
fprintf(stderr, "    Verify %s rc %d\n", tagName(dbi_rpmtag), rc);

    return rc;
}

INLINE int dbiClose(dbiIndex dbi, unsigned int flags) {
if (_debug < 0 || dbi->dbi_debug)
fprintf(stderr, "    Close %s\n", tagName(dbi->dbi_rpmtag));
    return (*dbi->dbi_vec->close) (dbi, flags);
}

dbiIndex dbiOpen(rpmdb db, int rpmtag, /*@unused@*/ unsigned int flags)
{
    int dbix;
    dbiIndex dbi = NULL;
    int _dbapi, _dbapi_rebuild, _dbapi_wanted;
    int rc = 0;

    if (db == NULL)
	return NULL;

    dbix = dbiTagToDbix(rpmtag);
    if (dbix < 0 || dbix >= dbiTagsMax)
	return NULL;

    /* Is this index already open ? */
    if ((dbi = db->_dbi[dbix]) != NULL)
	return dbi;

    _dbapi_rebuild = rpmExpandNumeric("%{_dbapi_rebuild}");
    if (_dbapi_rebuild < 1 || _dbapi_rebuild > 3)
	_dbapi_rebuild = 3;
    _dbapi_wanted = (_rebuildinprogress ? -1 : db->db_api);

    switch (_dbapi_wanted) {
    default:
	_dbapi = _dbapi_wanted;
	if (_dbapi < 0 || _dbapi >= 4 || mydbvecs[_dbapi] == NULL) {
	    return NULL;
	}
	errno = 0;
	dbi = NULL;
	rc = (*mydbvecs[_dbapi]->open) (db, rpmtag, &dbi);
	if (rc) {
	    static int _printed[32];
	    if (!_printed[dbix & 0x1f]++)
		rpmError(RPMERR_DBOPEN,
			_("cannot open %s index using db%d - %s (%d)\n"),
			tagName(rpmtag), _dbapi,
			(rc > 0 ? strerror(rc) : ""), rc);
	    _dbapi = -1;
	}
	break;
    case -1:
	_dbapi = 4;
	while (_dbapi-- > 1) {
	    if (mydbvecs[_dbapi] == NULL)
		continue;
	    errno = 0;
	    dbi = NULL;
	    rc = (*mydbvecs[_dbapi]->open) (db, rpmtag, &dbi);
	    if (rc == 0 && dbi)
		/*@loopbreak@*/ break;
	}
	if (_dbapi <= 0) {
	    static int _printed[32];
	    if (!_printed[dbix & 0x1f]++)
		rpmError(RPMERR_DBOPEN, _("cannot open %s index\n"),
			tagName(rpmtag));
	    rc = 1;
	    goto exit;
	}
	if (db->db_api == -1 && _dbapi > 0)
	    db->db_api = _dbapi;
    	break;
    }

    /* Require conversion. */
    if (rc && _dbapi_wanted >= 0 && _dbapi != _dbapi_wanted && _dbapi_wanted == _dbapi_rebuild) {
	rc = (_rebuildinprogress ? 0 : 1);
	goto exit;
    }

    /* Suggest possible configuration */
    if (_dbapi_wanted >= 0 && _dbapi != _dbapi_wanted) {
	rc = 1;
	goto exit;
    }

    /* Suggest possible configuration */
    if (_dbapi_wanted < 0 && _dbapi != _dbapi_rebuild) {
	rc = (_rebuildinprogress ? 0 : 1);
	goto exit;
    }

exit:
    if (rc == 0 && dbi)
	db->_dbi[dbix] = dbi;
    else
	dbi = db3Free(dbi);

    return dbi;
}

/**
 * Create and initialize item for index database set.
 * @param hdrNum	header instance in db
 * @param tagNum	tag index in header
 * @return		new item
 */
static INLINE dbiIndexItem dbiIndexNewItem(unsigned int hdrNum, unsigned int tagNum)
	/*@*/
{
    dbiIndexItem rec = xcalloc(1, sizeof(*rec));
    rec->hdrNum = hdrNum;
    rec->tagNum = tagNum;
    return rec;
}

union _dbswap {
    unsigned int ui;
    unsigned char uc[4];
};

#define	_DBSWAP(_a) \
  { unsigned char _b, *_c = (_a).uc; \
    _b = _c[3]; _c[3] = _c[0]; _c[0] = _b; \
    _b = _c[2]; _c[2] = _c[1]; _c[1] = _b; \
  }

/**
 * Return items that match criteria.
 * @param dbi		index database handle
 * @param dbcursor	index database cursor
 * @param keyp		search key
 * @param keylen	search key length (0 will use strlen(key))
 * @retval setp		address of items retrieved from index database
 * @return		-1 error, 0 success, 1 not found
 */
static int dbiSearch(dbiIndex dbi, DBC * dbcursor,
		const char * keyp, size_t keylen, /*@out@*/ dbiIndexSet * setp)
	/*@modifies *dbcursor, *setp, fileSystem @*/
{
    unsigned int gflags = 0;	/* dbiGet() flags */
    void * datap = NULL;
    size_t datalen = 0;
    int rc;

    if (setp) *setp = NULL;
    if (keylen == 0) keylen = strlen(keyp);

    /*@-mods@*/		/* FIX: indirection @*/
    rc = dbiGet(dbi, dbcursor, (void **)&keyp, &keylen, &datap, &datalen,
		gflags);
    /*@=mods@*/

    if (rc > 0) {
	rpmError(RPMERR_DBGETINDEX,
		_("error(%d) getting \"%s\" records from %s index\n"),
		rc, keyp, tagName(dbi->dbi_rpmtag));
    } else
    if (rc == 0 && setp) {
	int _dbbyteswapped = dbiByteSwapped(dbi);
	const char * sdbir = datap;
	dbiIndexSet set;
	int i;

	set = xmalloc(sizeof(*set));

	/* Convert to database internal format */
	if (sdbir)
	switch (dbi->dbi_jlen) {
	default:
	case 2*sizeof(int_32):
	    set->count = datalen / (2*sizeof(int_32));
	    set->recs = xmalloc(set->count * sizeof(*(set->recs)));
	    for (i = 0; i < set->count; i++) {
		union _dbswap hdrNum, tagNum;

		memcpy(&hdrNum.ui, sdbir, sizeof(hdrNum.ui));
		sdbir += sizeof(hdrNum.ui);
		memcpy(&tagNum.ui, sdbir, sizeof(tagNum.ui));
		sdbir += sizeof(tagNum.ui);
		if (_dbbyteswapped) {
		    _DBSWAP(hdrNum);
		    _DBSWAP(tagNum);
		}
		set->recs[i].hdrNum = hdrNum.ui;
		set->recs[i].tagNum = tagNum.ui;
		set->recs[i].fpNum = 0;
		set->recs[i].dbNum = 0;
	    }
	    break;
	case 1*sizeof(int_32):
	    set->count = datalen / (1*sizeof(int_32));
	    set->recs = xmalloc(set->count * sizeof(*(set->recs)));
	    for (i = 0; i < set->count; i++) {
		union _dbswap hdrNum;

		memcpy(&hdrNum.ui, sdbir, sizeof(hdrNum.ui));
		sdbir += sizeof(hdrNum.ui);
		if (_dbbyteswapped) {
		    _DBSWAP(hdrNum);
		}
		set->recs[i].hdrNum = hdrNum.ui;
		set->recs[i].tagNum = 0;
		set->recs[i].fpNum = 0;
		set->recs[i].dbNum = 0;
	    }
	    break;
	}
	if (setp) *setp = set;
    }
    return rc;
}

/**
 * Change/delete items that match criteria.
 * @param dbi		index database handle
 * @param dbcursor	index database cursor
 * @param keyp		update key
 * @param keylen	update key length
 * @param set		items to update in index database
 * @return		0 success, 1 not found
 */
/*@-compmempass -mustmod@*/
static int dbiUpdateIndex(dbiIndex dbi, DBC * dbcursor,
		const void * keyp, size_t keylen, dbiIndexSet set)
	/*@modifies *dbcursor, set, fileSystem @*/
{
    unsigned int pflags = 0;	/* dbiPut() flags */
    unsigned int dflags = 0;	/* dbiDel() flags */
    void * datap;
    size_t datalen;
    int rc;

    if (set->count) {
	char * tdbir;
	int i;
	int _dbbyteswapped = dbiByteSwapped(dbi);

	/* Convert to database internal format */

	switch (dbi->dbi_jlen) {
	default:
	case 2*sizeof(int_32):
	    datalen = set->count * (2 * sizeof(int_32));
	    datap = tdbir = alloca(datalen);
	    for (i = 0; i < set->count; i++) {
		union _dbswap hdrNum, tagNum;

		memset(&hdrNum, 0, sizeof(hdrNum));
		memset(&tagNum, 0, sizeof(tagNum));
		hdrNum.ui = set->recs[i].hdrNum;
		tagNum.ui = set->recs[i].tagNum;
		if (_dbbyteswapped) {
		    _DBSWAP(hdrNum);
		    _DBSWAP(tagNum);
		}
		memcpy(tdbir, &hdrNum.ui, sizeof(hdrNum.ui));
		tdbir += sizeof(hdrNum.ui);
		memcpy(tdbir, &tagNum.ui, sizeof(tagNum.ui));
		tdbir += sizeof(tagNum.ui);
	    }
	    break;
	case 1*sizeof(int_32):
	    datalen = set->count * (1 * sizeof(int_32));
	    datap = tdbir = alloca(datalen);
	    for (i = 0; i < set->count; i++) {
		union _dbswap hdrNum;

		memset(&hdrNum, 0, sizeof(hdrNum));
		hdrNum.ui = set->recs[i].hdrNum;
		if (_dbbyteswapped) {
		    _DBSWAP(hdrNum);
		}
		memcpy(tdbir, &hdrNum.ui, sizeof(hdrNum.ui));
		tdbir += sizeof(hdrNum.ui);
	    }
	    break;
	}

	rc = dbiPut(dbi, dbcursor, keyp, keylen, datap, datalen, pflags);

	if (rc) {
	    rpmError(RPMERR_DBPUTINDEX,
		_("error(%d) storing record %s into %s\n"),
		rc, keyp, tagName(dbi->dbi_rpmtag));
	}

    } else {

	rc = dbiDel(dbi, dbcursor, keyp, keylen, dflags);

	if (rc) {
	    rpmError(RPMERR_DBPUTINDEX,
		_("error(%d) removing record %s from %s\n"),
		rc, keyp, tagName(dbi->dbi_rpmtag));
	}

    }

    return rc;
}
/*@=compmempass =mustmod@*/

/* XXX assumes hdrNum is first int in dbiIndexItem */
static int hdrNumCmp(const void * one, const void * two)
	/*@*/
{
    const int * a = one, * b = two;
    return (*a - *b);
}

/**
 * Append element(s) to set of index database items.
 * @param set		set of index database items
 * @param recs		array of items to append to set
 * @param nrecs		number of items
 * @param recsize	size of an array item
 * @param sortset	should resulting set be sorted?
 * @return		0 success, 1 failure (bad args)
 */
static INLINE int dbiAppendSet(dbiIndexSet set, const void * recs,
	int nrecs, size_t recsize, int sortset)
	/*@modifies set @*/
{
    const char * rptr = recs;
    size_t rlen = (recsize < sizeof(*(set->recs)))
		? recsize : sizeof(*(set->recs));

    if (set == NULL || recs == NULL || nrecs <= 0 || recsize <= 0)
	return 1;

    if (set->count == 0)
	set->recs = xmalloc(nrecs * sizeof(*(set->recs)));
    else
	set->recs = xrealloc(set->recs,
				(set->count + nrecs) * sizeof(*(set->recs)));

    memset(set->recs + set->count, 0, nrecs * sizeof(*(set->recs)));

    while (nrecs-- > 0) {
	/*@-mayaliasunique@*/
	memcpy(set->recs + set->count, rptr, rlen);
	/*@=mayaliasunique@*/
	rptr += recsize;
	set->count++;
    }

    if (set->count > 1 && sortset)
	qsort(set->recs, set->count, sizeof(*(set->recs)), hdrNumCmp);

    return 0;
}

/**
 * Remove element(s) from set of index database items.
 * @param set		set of index database items
 * @param recs		array of items to remove from set
 * @param nrecs		number of items
 * @param recsize	size of an array item
 * @param sorted	array is already sorted?
 * @return		0 success, 1 failure (no items found)
 */
static INLINE int dbiPruneSet(dbiIndexSet set, void * recs, int nrecs,
		size_t recsize, int sorted)
	/*@modifies set, recs @*/
{
    int from;
    int to = 0;
    int num = set->count;
    int numCopied = 0;

    if (nrecs > 1 && !sorted)
	qsort(recs, nrecs, recsize, hdrNumCmp);

    for (from = 0; from < num; from++) {
	if (bsearch(&set->recs[from], recs, nrecs, recsize, hdrNumCmp)) {
	    set->count--;
	    continue;
	}
	if (from != to)
	    set->recs[to] = set->recs[from]; /* structure assignment */
	to++;
	numCopied++;
    }

    return (numCopied == num);
}

/* XXX transaction.c */
unsigned int dbiIndexSetCount(dbiIndexSet set) {
    return set->count;
}

/* XXX transaction.c */
unsigned int dbiIndexRecordOffset(dbiIndexSet set, int recno) {
    return set->recs[recno].hdrNum;
}

/* XXX transaction.c */
unsigned int dbiIndexRecordFileNumber(dbiIndexSet set, int recno) {
    return set->recs[recno].tagNum;
}

/* XXX transaction.c */
dbiIndexSet dbiFreeIndexSet(dbiIndexSet set) {
    if (set) {
	set->recs = _free(set->recs);
	set = _free(set);
    }
    return set;
}

/**
 * Disable all signals, returning previous signal mask.
 */
static int blockSignals(/*@unused@*/ rpmdb db, /*@out@*/ sigset_t * oldMask)
	/*@modifies *oldMask, internalState @*/
{
    sigset_t newMask;

    (void) sigfillset(&newMask);		/* block all signals */
    return sigprocmask(SIG_BLOCK, &newMask, oldMask);
}

/**
 * Restore signal mask.
 */
static int unblockSignals(/*@unused@*/ rpmdb db, sigset_t * oldMask)
	/*@modifies internalState @*/
{
    return sigprocmask(SIG_SETMASK, oldMask, NULL);
}

#define	_DB_ROOT	"/"
#define	_DB_HOME	"%{_dbpath}"
#define	_DB_FLAGS	0
#define _DB_MODE	0
#define _DB_PERMS	0644

#define _DB_MAJOR	-1
#define	_DB_REMOVE_ENV	0
#define	_DB_FILTER_DUPS	0
#define	_DB_ERRPFX	"rpmdb"

/*@-fullinitblock@*/
/*@observer@*/ static struct rpmdb_s dbTemplate = {
    _DB_ROOT,	_DB_HOME, _DB_FLAGS, _DB_MODE, _DB_PERMS,
    _DB_MAJOR,	_DB_REMOVE_ENV, _DB_FILTER_DUPS, _DB_ERRPFX
};
/*@=fullinitblock@*/

int rpmdbOpenAll(rpmdb db)
{
    int dbix;
    int rc = 0;

    if (db == NULL) return -2;

    if (dbiTags != NULL)
    for (dbix = 0; dbix < dbiTagsMax; dbix++) {
	if (db->_dbi[dbix] != NULL)
	    continue;
	(void) dbiOpen(db, dbiTags[dbix], db->db_flags);
    }
    return rc;
}

/* XXX query.c, rpminstall.c, verify.c */
int rpmdbClose(rpmdb db)
{
    int dbix;
    int rc = 0;

    if (db == NULL) return 0;
    if (db->_dbi)
    for (dbix = db->db_ndbi; --dbix >= 0; ) {
	int xx;
	if (db->_dbi[dbix] == NULL)
	    continue;
	/*@-unqualifiedtrans@*/		/* FIX: double indirection. */
    	xx = dbiClose(db->_dbi[dbix], 0);
	if (xx && rc == 0) rc = xx;
    	db->_dbi[dbix] = NULL;
	/*@=unqualifiedtrans@*/
    }
    db->db_errpfx = _free(db->db_errpfx);
    db->db_root = _free(db->db_root);
    db->db_home = _free(db->db_home);
    db->_dbi = _free(db->_dbi);
    db = _free(db);
    return rc;
}

int rpmdbSync(rpmdb db)
{
    int dbix;
    int rc = 0;

    if (db == NULL) return 0;
    for (dbix = 0; dbix < db->db_ndbi; dbix++) {
	int xx;
	if (db->_dbi[dbix] == NULL)
	    continue;
    	xx = dbiSync(db->_dbi[dbix], 0);
	if (xx && rc == 0) rc = xx;
    }
    return rc;
}

static /*@only@*/ /*@null@*/
rpmdb newRpmdb(/*@kept@*/ /*@null@*/ const char * root,
		/*@kept@*/ /*@null@*/ const char * home,
		int mode, int perms, int flags)
	/*@modifies _db_filter_dups @*/
{
    rpmdb db = xcalloc(sizeof(*db), 1);
    const char * epfx = _DB_ERRPFX;
    static int _initialized = 0;

    if (!_initialized) {
	_db_filter_dups = rpmExpandNumeric("%{_filterdbdups}");
	_initialized = 1;
    }

    /*@-assignexpose@*/
    *db = dbTemplate;	/* structure assignment */
    /*@=assignexpose@*/

    if (!(perms & 0600)) perms = 0644;	/* XXX sanity */

    if (mode >= 0)	db->db_mode = mode;
    if (perms >= 0)	db->db_perms = perms;
    if (flags >= 0)	db->db_flags = flags;

    /*@-nullpass@*/
    db->db_root = rpmGetPath( (root && *root ? root : _DB_ROOT), NULL);
    db->db_home = rpmGetPath( (home && *home ? home : _DB_HOME), NULL);
    /*@=nullpass@*/
    if (!(db->db_home && db->db_home[0] != '%')) {
	rpmError(RPMERR_DBOPEN, _("no dbpath has been set\n"));
	(void) rpmdbClose(db);
	/*@-globstate@*/ return NULL; /*@=globstate@*/
    }
    /*@-nullpass@*/
    db->db_errpfx = rpmExpand( (epfx && *epfx ? epfx : _DB_ERRPFX), NULL);
    /*@=nullpass@*/
    db->db_remove_env = 0;
    db->db_filter_dups = _db_filter_dups;
    db->db_ndbi = dbiTagsMax;
    db->_dbi = xcalloc(db->db_ndbi, sizeof(*db->_dbi));
    /*@-globstate@*/ return db; /*@=globstate@*/
}

static int openDatabase(/*@null@*/ const char * prefix,
		/*@null@*/ const char * dbpath,
		int _dbapi, /*@null@*/ /*@out@*/ rpmdb *dbp,
		int mode, int perms, int flags)
	/*@modifies *dbp, fileSystem @*/
{
    rpmdb db;
    int rc;
    unsigned int gflags = 0;	/* dbiGet() flags */
    static int _initialized = 0;
    int justCheck = flags & RPMDB_FLAG_JUSTCHECK;
    int minimal = flags & RPMDB_FLAG_MINIMAL;

    if (!_initialized || dbiTagsMax == 0) {
	dbiTagsInit();
	_initialized++;
    }

    /* Insure that _dbapi has one of -1, 1, 2, or 3 */
    if (_dbapi < -1 || _dbapi > 3)
	_dbapi = -1;
    if (_dbapi == 0)
	_dbapi = 1;

    if (dbp)
	*dbp = NULL;
    if (mode & O_WRONLY) 
	return 1;

    db = newRpmdb(prefix, dbpath, mode, perms, flags);
    if (db == NULL)
	return 1;
    db->db_api = _dbapi;

    {	int dbix;

	rc = 0;
	if (dbiTags != NULL)
	for (dbix = 0; rc == 0 && dbix < dbiTagsMax; dbix++) {
	    dbiIndex dbi;
	    int rpmtag;

	    /* Filter out temporary databases */
	    switch ((rpmtag = dbiTags[dbix])) {
	    case RPMDBI_AVAILABLE:
	    case RPMDBI_ADDED:
	    case RPMDBI_REMOVED:
	    case RPMDBI_DEPENDS:
		continue;
		/*@notreached@*/ break;
	    default:
		break;
	    }

	    dbi = dbiOpen(db, rpmtag, 0);
	    if (dbi == NULL) {
		rc = -2;
		break;
	    }

	    switch (rpmtag) {
	    case RPMDBI_PACKAGES:
		if (dbi == NULL) rc |= 1;
		/* XXX open only Packages, indices created on the fly. */
#if 0
		if (db->db_api == 3)
#endif
		    goto exit;
		/*@notreached@*/ break;
	    case RPMTAG_NAME:
		if (dbi == NULL) rc |= 1;
		if (minimal)
		    goto exit;
		break;
	    case RPMTAG_BASENAMES:
	    {	void * keyp = NULL;
		DBC * dbcursor;
		int xx;

    /* We used to store the fileindexes as complete paths, rather then
       plain basenames. Let's see which version we are... */
    /*
     * XXX FIXME: db->fileindex can be NULL under pathological (e.g. mixed
     * XXX db1/db2 linkage) conditions.
     */
		if (justCheck)
		    break;
		dbcursor = NULL;
		xx = dbiCopen(dbi, &dbcursor, 0);
		xx = dbiGet(dbi, dbcursor, &keyp, NULL, NULL, NULL, gflags);
		if (xx == 0) {
		    const char * akey = keyp;
		    if (akey && strchr(akey, '/')) {
			rpmError(RPMERR_OLDDB, _("old format database is present; "
				"use --rebuilddb to generate a new format database\n"));
			rc |= 1;
		    }
		}
		xx = dbiCclose(dbi, dbcursor, 0);
		dbcursor = NULL;
	    }	break;
	    default:
		break;
	    }
	}
    }

exit:
    if (rc || justCheck || dbp == NULL)
	(void) rpmdbClose(db);
    else
	*dbp = db;

    return rc;
}

/* XXX python/rpmmodule.c */
int rpmdbOpen (const char * prefix, rpmdb *dbp, int mode, int perms)
{
    int _dbapi = rpmExpandNumeric("%{_dbapi}");
    return openDatabase(prefix, NULL, _dbapi, dbp, mode, perms, 0);
}

int rpmdbInit (const char * prefix, int perms)
{
    rpmdb db = NULL;
    int _dbapi = rpmExpandNumeric("%{_dbapi}");
    int rc;

    rc = openDatabase(prefix, NULL, _dbapi, &db, (O_CREAT | O_RDWR),
		perms, RPMDB_FLAG_JUSTCHECK);
    if (db != NULL) {
	int xx;
	xx = rpmdbOpenAll(db);
	if (xx && rc == 0) rc = xx;
	xx = rpmdbClose(db);
	if (xx && rc == 0) rc = xx;
	db = NULL;
    }
    return rc;
}

int rpmdbVerify(const char * prefix)
{
    rpmdb db = NULL;
    int _dbapi = rpmExpandNumeric("%{_dbapi}");
    int rc = 0;

    rc = openDatabase(prefix, NULL, _dbapi, &db, O_RDONLY, 0644, 0);
    if (rc) return rc;

    if (db != NULL) {
	int dbix;
	int xx;
	rc = rpmdbOpenAll(db);

	for (dbix = db->db_ndbi; --dbix >= 0; ) {
	    if (db->_dbi[dbix] == NULL)
		continue;
	    /*@-unqualifiedtrans@*/		/* FIX: double indirection. */
	    xx = dbiVerify(db->_dbi[dbix], 0);
	    if (xx && rc == 0) rc = xx;
	    db->_dbi[dbix] = NULL;
	    /*@=unqualifiedtrans@*/
	}

	/*@-nullstate@*/	/* FIX: db->_dbi[] may be NULL. */
	xx = rpmdbClose(db);
	/*@=nullstate@*/
	if (xx && rc == 0) rc = xx;
	db = NULL;
    }
    return rc;
}

static int rpmdbFindByFile(rpmdb db, /*@null@*/ const char * filespec,
			/*@out@*/ dbiIndexSet * matches)
	/*@modifies db, *matches, fileSystem @*/
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    const char * dirName;
    const char * baseName;
    rpmTagType bnt, dnt;
    fingerPrintCache fpc;
    fingerPrint fp1;
    dbiIndex dbi = NULL;
    DBC * dbcursor;
    dbiIndexSet allMatches = NULL;
    dbiIndexItem rec = NULL;
    int i;
    int rc;
    int xx;

    *matches = NULL;
    if (filespec == NULL) return -2;
    if ((baseName = strrchr(filespec, '/')) != NULL) {
    	char * t;
	size_t len;

    	len = baseName - filespec + 1;
	t = strncpy(alloca(len + 1), filespec, len);
	t[len] = '\0';
	dirName = t;
	baseName++;
    } else {
	dirName = "";
	baseName = filespec;
    }
    if (baseName == NULL)
	return -2;

    fpc = fpCacheCreate(20);
    fp1 = fpLookup(fpc, dirName, baseName, 1);

    dbi = dbiOpen(db, RPMTAG_BASENAMES, 0);
    if (dbi != NULL) {
	dbcursor = NULL;
	xx = dbiCopen(dbi, &dbcursor, 0);
	rc = dbiSearch(dbi, dbcursor, baseName, strlen(baseName), &allMatches);
	xx = dbiCclose(dbi, dbcursor, 0);
	dbcursor = NULL;
    } else
	rc = -2;

    if (rc) {
	allMatches = dbiFreeIndexSet(allMatches);
	fpCacheFree(fpc);
	return rc;
    }

    *matches = xcalloc(1, sizeof(**matches));
    rec = dbiIndexNewItem(0, 0);
    i = 0;
    if (allMatches != NULL)
    while (i < allMatches->count) {
	const char ** baseNames, ** dirNames;
	int_32 * dirIndexes;
	unsigned int offset = dbiIndexRecordOffset(allMatches, i);
	unsigned int prevoff;
	Header h;

	{   rpmdbMatchIterator mi;
	    mi = rpmdbInitIterator(db, RPMDBI_PACKAGES, &offset, sizeof(offset));
	    h = rpmdbNextIterator(mi);
	    if (h)
		h = headerLink(h);
	    mi = rpmdbFreeIterator(mi);
	}

	if (h == NULL) {
	    i++;
	    continue;
	}

	(void) hge(h, RPMTAG_BASENAMES, &bnt, (void **) &baseNames, NULL);
	(void) hge(h, RPMTAG_DIRNAMES, &dnt, (void **) &dirNames, NULL);
	(void) hge(h, RPMTAG_DIRINDEXES, NULL, (void **) &dirIndexes, NULL);

	do {
	    fingerPrint fp2;
	    int num = dbiIndexRecordFileNumber(allMatches, i);

	    fp2 = fpLookup(fpc, dirNames[dirIndexes[num]], baseNames[num], 1);
	    /*@-nullpass@*/
	    if (FP_EQUAL(fp1, fp2)) {
	    /*@=nullpass@*/
		rec->hdrNum = dbiIndexRecordOffset(allMatches, i);
		rec->tagNum = dbiIndexRecordFileNumber(allMatches, i);
		(void) dbiAppendSet(*matches, rec, 1, sizeof(*rec), 0);
	    }

	    prevoff = offset;
	    i++;
	    offset = dbiIndexRecordOffset(allMatches, i);
	} while (i < allMatches->count && 
		(i == 0 || offset == prevoff));

	baseNames = hfd(baseNames, bnt);
	dirNames = hfd(dirNames, dnt);
	h = headerFree(h);
    }

    rec = _free(rec);
    allMatches = dbiFreeIndexSet(allMatches);

    fpCacheFree(fpc);

    if ((*matches)->count == 0) {
	*matches = dbiFreeIndexSet(*matches);
	return 1;
    }

    return 0;
}

/* XXX python/upgrade.c, install.c, uninstall.c */
int rpmdbCountPackages(rpmdb db, const char * name)
{
    dbiIndex dbi;
    dbiIndexSet matches = NULL;
    int rc = -1;
    int xx;

    /* XXX
     * There's a segfault here with CDB access, let's treat the symptom
     * while diagnosing the disease.
     */
    if (name == NULL || *name == '\0')
	return 0;

    dbi = dbiOpen(db, RPMTAG_NAME, 0);
    if (dbi) {
	DBC * dbcursor = NULL;
	xx = dbiCopen(dbi, &dbcursor, 0);
	rc = dbiSearch(dbi, dbcursor, name, strlen(name), &matches);
	xx = dbiCclose(dbi, dbcursor, 0);
	dbcursor = NULL;
    }

    if (rc == 0)	/* success */
	/*@-nullpass@*/
	rc = dbiIndexSetCount(matches);
	/*@=nullpass@*/
    else if (rc > 0)	/* error */
	rpmError(RPMERR_DBCORRUPT, _("error(%d) counting packages\n"), rc);
    else		/* not found */
	rc = 0;

    matches = dbiFreeIndexSet(matches);

    return rc;
}

/* XXX transaction.c */
/* 0 found matches */
/* 1 no matches */
/* 2 error */
/**
 * Attempt partial matches on name[-version[-release]] strings.
 * @param dbi		index database handle (always RPMTAG_NAME)
 * @param dbcursor	index database cursor
 * @param name		package name
 * @param version	package version (can be a regex pattern)
 * @param release	package release (can be a regex pattern)
 * @retval matches	set of header instances that match
 * @return 		0 on match, 1 on no match, 2 on error
 */
static int dbiFindMatches(dbiIndex dbi, DBC * dbcursor,
		const char * name,
		/*@null@*/ const char * version,
		/*@null@*/ const char * release,
		/*@out@*/ dbiIndexSet * matches)
	/*@modifies dbi, *dbcursor, *matches, fileSystem @*/
{
    int gotMatches;
    int rc;
    int i;

    rc = dbiSearch(dbi, dbcursor, name, strlen(name), matches);

    if (rc != 0) {
	rc = ((rc == -1) ? 2 : 1);
	goto exit;
    }

    if (version == NULL && release == NULL) {
	rc = 0;
	goto exit;
    }

    gotMatches = 0;

    /* make sure the version and releases match */
    for (i = 0; i < dbiIndexSetCount(*matches); i++) {
	unsigned int recoff = dbiIndexRecordOffset(*matches, i);
	Header h;

	if (recoff == 0)
	    continue;

	{   rpmdbMatchIterator mi;
	    mi = rpmdbInitIterator(dbi->dbi_rpmdb,
			RPMDBI_PACKAGES, &recoff, sizeof(recoff));

	    /* Set iterator selectors for version/release if available. */
	    if (version && rpmdbSetIteratorRE(mi, RPMTAG_VERSION, version)) {
		rc = 2;
		goto exit;
	    }
	    if (release && rpmdbSetIteratorRE(mi, RPMTAG_RELEASE, release)) {
		rc = 2;
		goto exit;
	    }

	    h = rpmdbNextIterator(mi);
	    if (h)
		h = headerLink(h);
	    mi = rpmdbFreeIterator(mi);
	}

	if (h)	/* structure assignment */
	    (*matches)->recs[gotMatches++] = (*matches)->recs[i];
	else
	    (*matches)->recs[i].hdrNum = 0;

	h = headerFree(h);
    }

    if (gotMatches) {
	(*matches)->count = gotMatches;
	rc = 0;
    } else
	rc = 1;

exit:
    if (rc && matches && *matches) {
	/*@-unqualifiedtrans@*/		/* FIX: double indirection */
	*matches = dbiFreeIndexSet(*matches);
	/*@=unqualifiedtrans@*/
    }
    return rc;
}

/**
 * Lookup by name, name-version, and finally by name-version-release.
 * Both version and release can be regex patterns.
 * @todo Name must be an exact match, as name is a db key.
 * @todo N-V-R split needs to be made RE aware, i.e. '[0-9]' is split badly.
 * @param dbi		index database handle (always RPMTAG_NAME)
 * @param dbcursor	index database cursor
 * @param arg		name[-version[-release]] string
 * @retval matches	set of header instances that match
 * @return 		0 on match, 1 on no match, 2 on error
 */
static int dbiFindByLabel(dbiIndex dbi, DBC * dbcursor,
		/*@null@*/ const char * arg, /*@out@*/ dbiIndexSet * matches)
	/*@modifies dbi, *dbcursor, *matches, fileSystem @*/
{
    char * localarg, * chptr;
    char * release;
    int rc;
 
    if (arg == NULL || strlen(arg) == 0) return 1;

    /* did they give us just a name? */
    rc = dbiFindMatches(dbi, dbcursor, arg, NULL, NULL, matches);
    if (rc != 1) return rc;

    /*@-unqualifiedtrans@*/
    *matches = dbiFreeIndexSet(*matches);
    /*@=unqualifiedtrans@*/

    /* maybe a name and a release */
    localarg = alloca(strlen(arg) + 1);
    strcpy(localarg, arg);

    chptr = (localarg + strlen(localarg)) - 1;
    while (chptr > localarg && *chptr != '-') chptr--;
    /*@-nullstate@*/	/* FIX: *matches may be NULL. */
    if (chptr == localarg) return 1;

    *chptr = '\0';
    rc = dbiFindMatches(dbi, dbcursor, localarg, chptr + 1, NULL, matches);
    if (rc != 1) return rc;

    /*@-unqualifiedtrans@*/
    *matches = dbiFreeIndexSet(*matches);
    /*@=unqualifiedtrans@*/
    
    /* how about name-version-release? */

    release = chptr + 1;
    while (chptr > localarg && *chptr != '-') chptr--;
    if (chptr == localarg) return 1;

    *chptr = '\0';
    return dbiFindMatches(dbi, dbcursor, localarg, chptr + 1, release, matches);
    /*@=nullstate@*/
}

/**
 * Rewrite a header in the database.
 *   Note: this is called from a markReplacedFiles iteration, and *must*
 *   preserve the "join key" (i.e. offset) for the header.
 * @param dbi		index database handle (always RPMDBI_PACKAGES)
 * @param dbcursor	index database cursor
 * @param offset	join key
 * @param h		rpm header
 * @return 		0 on success
 */
static int dbiUpdateRecord(dbiIndex dbi, DBC * dbcursor, int offset, Header h)
	/*@modifies *dbcursor, h, fileSystem @*/
{
    sigset_t signalMask;
    void * uh;
    size_t uhlen;
    int rc = EINVAL;	/* XXX W2DO? */
    unsigned int pflags = 0;	/* dbiPut() flags */
    int xx;

    if (_noDirTokens)
	expandFilelist(h);

    uhlen = headerSizeof(h, HEADER_MAGIC_NO);
    uh = headerUnload(h);
    if (uh) {
	(void) blockSignals(dbi->dbi_rpmdb, &signalMask);
	rc = dbiPut(dbi, dbcursor, &offset, sizeof(offset), uh, uhlen, pflags);
	xx = dbiSync(dbi, 0);
	(void) unblockSignals(dbi->dbi_rpmdb, &signalMask);
	uh = _free(uh);
    }
    return rc;
}

typedef struct miRE_s {
    rpmTag		tag;		/*!< header tag */
/*@only@*/ const char *	pattern;	/*!< regular expression */
    int			cflags;		/*!< compile flags */
/*@only@*/ regex_t *	preg;		/*!< compiled pattern buffer */
    int			eflags;		/*!< match flags */
} * miRE;

struct _rpmdbMatchIterator {
/*@only@*/ const void *	mi_keyp;
    size_t		mi_keylen;
/*@kept@*/ rpmdb	mi_rpmdb;
    int			mi_rpmtag;
    dbiIndexSet		mi_set;
    DBC *		mi_dbc;
    unsigned int	mi_ndups;
    int			mi_setx;
/*@null@*/ Header	mi_h;
    int			mi_sorted;
    int			mi_cflags;
    int			mi_modified;
    unsigned int	mi_prevoffset;
    unsigned int	mi_offset;
    unsigned int	mi_filenum;
    unsigned int	mi_fpnum;
    unsigned int	mi_dbnum;
    int			mi_nre;
/*@only@*//*@null@*/ miRE mi_re;
/*@only@*//*@null@*/ const char * mi_version;
/*@only@*//*@null@*/ const char * mi_release;
};

rpmdbMatchIterator rpmdbFreeIterator(rpmdbMatchIterator mi)
{
    dbiIndex dbi = NULL;
    int xx;
    int i;

    if (mi == NULL)
	return mi;

    dbi = dbiOpen(mi->mi_rpmdb, RPMDBI_PACKAGES, 0);
    if (mi->mi_h) {
	if (dbi && mi->mi_dbc && mi->mi_modified && mi->mi_prevoffset) {
	    xx = dbiUpdateRecord(dbi, mi->mi_dbc, mi->mi_prevoffset, mi->mi_h);
	}
	mi->mi_h = headerFree(mi->mi_h);
    }
    if (dbi) {
	if (dbi->dbi_rmw)
	    xx = dbiCclose(dbi, dbi->dbi_rmw, 0);
	dbi->dbi_rmw = NULL;
    }

    if (mi->mi_re != NULL)
    for (i = 0; i < mi->mi_nre; i++) {
	miRE mire = mi->mi_re + i;
	mire->pattern = _free(mire->pattern);
	if (mire->preg != NULL) {
	    regfree(mire->preg);
	    mire->preg = _free(mire->preg);
	}
    }
    mi->mi_re = _free(mi->mi_re);

    mi->mi_release = _free(mi->mi_release);
    mi->mi_version = _free(mi->mi_version);
    if (dbi && mi->mi_dbc)
	xx = dbiCclose(dbi, mi->mi_dbc, DBI_ITERATOR);
    mi->mi_dbc = NULL;
    mi->mi_set = dbiFreeIndexSet(mi->mi_set);
    mi->mi_keyp = _free(mi->mi_keyp);
    mi = _free(mi);
    return mi;
}

rpmdb rpmdbGetIteratorRpmDB(rpmdbMatchIterator mi) {
    if (mi == NULL)
	return NULL;
    /*@-retexpose -retalias@*/
    return mi->mi_rpmdb;
    /*@=retexpose =retalias@*/
}

unsigned int rpmdbGetIteratorOffset(rpmdbMatchIterator mi) {
    if (mi == NULL)
	return 0;
    return mi->mi_offset;
}

unsigned int rpmdbGetIteratorFileNum(rpmdbMatchIterator mi) {
    if (mi == NULL)
	return 0;
    return mi->mi_filenum;
}

int rpmdbGetIteratorCount(rpmdbMatchIterator mi) {
    if (!(mi && mi->mi_set))
	return 0;	/* XXX W2DO? */
    return mi->mi_set->count;
}

/**
 * Return pattern match.
 * @param mi		rpm database iterator
 * @return		0 if pattern matches
 */
static int miregexec(miRE mire, const char * val)
	/*@*/
{
    int rc = regexec(mire->preg, val, 0, NULL, mire->eflags);

    if (rc && rc != REG_NOMATCH) {
	char msg[256];
	(void) regerror(rc, mire->preg, msg, sizeof(msg)-1);
	msg[sizeof(msg)-1] = '\0';
	rpmError(RPMERR_REGEXEC, "%s: regexec failed: %s\n", mire->pattern, msg);
    }
    return rc;
}

/**
 * Compare iterator selctors by rpm tag (qsort/bsearch).
 * @param a		1st iterator selector
 * @param b		2nd iterator selector
 * @return		result of comparison
 */
static int mireCmp(const void * a, const void * b)
{
    const miRE mireA = (const miRE) a;
    const miRE mireB = (const miRE) b;
    return (mireA->tag - mireB->tag);
}

int rpmdbSetIteratorRE(rpmdbMatchIterator mi, rpmTag tag, const char * pattern)
{
    const char * allpat = NULL;
    regex_t * preg = NULL;
    miRE mire = NULL;
    int cflags = (REG_EXTENDED | REG_NOSUB);
    int eflags = 0;
    int rc = 0;

    if (mi == NULL || pattern == NULL)
	return rc;

    {	int nb = strlen(pattern);
	char * t = xmalloc(nb + sizeof("^.$"));
	allpat = t;
	if (pattern[0] != '^') *t++ = '^';

	/* XXX let's fix a common error with RE's */
	if (pattern[0] == '*') *t++ = '.';

	t = stpcpy(t, pattern);
	if (pattern[nb-1] != '$') *t++ = '$';
	*t = '\0';
    }
    preg = xcalloc(1, sizeof(*preg));
    rc = regcomp(preg, allpat, cflags);
    if (rc) {
	char msg[256];
	(void) regerror(rc, preg, msg, sizeof(msg)-1);
	msg[sizeof(msg)-1] = '\0';
	rpmError(RPMERR_REGCOMP, "%s: regcomp failed: %s\n", allpat, msg);
	goto exit;
    }

    mi->mi_re = xrealloc(mi->mi_re, (mi->mi_nre + 1) * sizeof(*mi->mi_re));
    mire = mi->mi_re + mi->mi_nre;
    mi->mi_nre++;
    
    mire->tag = tag;
    mire->pattern = allpat;
    mire->cflags = cflags;
    mire->preg = preg;
    mire->eflags = eflags;

    (void) qsort(mi->mi_re, mi->mi_nre, sizeof(*mi->mi_re), mireCmp);

exit:
    if (rc) {
	/*@-kepttrans@*/	/* FIX: mire has kept values */
	allpat = _free(allpat);
	regfree(preg);
	preg = _free(preg);
	/*@=kepttrans@*/
    }
    return rc;
}

/**
 * Return iterator selector match.
 * @param mi		rpm database iterator
 * @return		1 if header should be skipped
 */
static int mireSkip (const rpmdbMatchIterator mi)
	/*@*/
{

    HGE_t hge = (HGE_t) headerGetEntryMinMemory;
    HFD_t hfd = (HFD_t) headerFreeData;
    union {
	void * ptr;
	const char ** argv;
	const char * str;
	int_32 * i32p;
	int_16 * i16p;
	int_8 * i8p;
    } u;
    char numbuf[32];
    rpmTagType t;
    int_32 c;
    miRE mire;
    int ntags = 0;
    int nmatches = 0;
    int i, j;

    if (mi->mi_h == NULL)	/* XXX can't happen */
	return 0;

    /*
     * Apply tag tests, implictly "||" for multiple patterns/values of a
     * single tag, implictly "&&" between multiple tag patterns.
     */
    if ((mire = mi->mi_re) != NULL)
    for (i = 0; i < mi->mi_nre; i++, mire++) {
	int anymatch;

	if (!hge(mi->mi_h, mire->tag, &t, (void **)&u, &c))
	    continue;

	anymatch = 0;		/* no matches yet */
	while (1) {
	    switch (t) {
	    case RPM_CHAR_TYPE:
	    case RPM_INT8_TYPE:
		sprintf(numbuf, "%d", (int) *u.i8p);
		if (!miregexec(mire, numbuf))
		    anymatch++;
		break;
	    case RPM_INT16_TYPE:
		sprintf(numbuf, "%d", (int) *u.i16p);
		if (!miregexec(mire, numbuf))
		    anymatch++;
		break;
	    case RPM_INT32_TYPE:
		sprintf(numbuf, "%d", (int) *u.i32p);
		if (!miregexec(mire, numbuf))
		    anymatch++;
		break;
	    case RPM_STRING_TYPE:
		if (!miregexec(mire, u.str))
		    anymatch++;
		break;
	    case RPM_I18NSTRING_TYPE:
	    case RPM_STRING_ARRAY_TYPE:
		for (j = 0; j < c; j++) {
		    if (miregexec(mire, u.argv[j]))
			continue;
		    anymatch++;
		    /*@innerbreak@*/ break;
		}
		break;
	    case RPM_NULL_TYPE:
	    case RPM_BIN_TYPE:
	    default:
		break;
	    }
	    if ((i+1) < mi->mi_nre && mire[0].tag == mire[1].tag) {
		i++;
		mire++;
		continue;
	    }
	    /*@innerbreak@*/ break;
	}

	u.ptr = hfd(u.ptr, t);

	ntags++;
	if (anymatch)
	    nmatches++;
    }

    return (ntags == nmatches ? 0 : 1);

}

int rpmdbSetIteratorRelease(rpmdbMatchIterator mi, const char * release) {
    return rpmdbSetIteratorRE(mi, RPMTAG_RELEASE, release);
}

int rpmdbSetIteratorVersion(rpmdbMatchIterator mi, const char * version) {
    return rpmdbSetIteratorRE(mi, RPMTAG_VERSION, version);
}

int rpmdbSetIteratorRewrite(rpmdbMatchIterator mi, int rewrite) {
    int rc;
    if (mi == NULL)
	return 0;
    rc = (mi->mi_cflags & DBI_WRITECURSOR) ? 1 : 0;
    if (rewrite)
	mi->mi_cflags |= DBI_WRITECURSOR;
    else
	mi->mi_cflags &= ~DBI_WRITECURSOR;
    return rc;
}

int rpmdbSetIteratorModified(rpmdbMatchIterator mi, int modified) {
    int rc;
    if (mi == NULL)
	return 0;
    rc = mi->mi_modified;
    mi->mi_modified = modified;
    return rc;
}

Header rpmdbNextIterator(rpmdbMatchIterator mi)
{
    dbiIndex dbi;
    void * uh = NULL;
    size_t uhlen = 0;
    unsigned int gflags = 0;	/* dbiGet() flags */
    void * keyp;
    size_t keylen;
    int rc;
    int xx;

    if (mi == NULL)
	return NULL;

    dbi = dbiOpen(mi->mi_rpmdb, RPMDBI_PACKAGES, 0);
    if (dbi == NULL)
	return NULL;

    /*
     * Cursors are per-iterator, not per-dbi, so get a cursor for the
     * iterator on 1st call. If the iteration is to rewrite headers, and the
     * CDB model is used for the database, then the cursor needs to
     * marked with DB_WRITECURSOR as well.
     */
    if (mi->mi_dbc == NULL)
	xx = dbiCopen(dbi, &mi->mi_dbc, (mi->mi_cflags | DBI_ITERATOR));
    dbi->dbi_lastoffset = mi->mi_prevoffset;

top:
    /* XXX skip over instances with 0 join key */
    do {
	if (mi->mi_set) {
	    if (!(mi->mi_setx < mi->mi_set->count))
		return NULL;
	    mi->mi_offset = dbiIndexRecordOffset(mi->mi_set, mi->mi_setx);
	    mi->mi_filenum = dbiIndexRecordFileNumber(mi->mi_set, mi->mi_setx);
	    keyp = &mi->mi_offset;
	    keylen = sizeof(mi->mi_offset);
	} else {
	    keyp = (void *)mi->mi_keyp;		/* XXX FIXME const */
	    keylen = mi->mi_keylen;

	    rc = dbiGet(dbi, mi->mi_dbc, &keyp, &keylen, &uh, &uhlen, gflags);
if (dbi->dbi_api == 1 && dbi->dbi_rpmtag == RPMDBI_PACKAGES && rc == EFAULT) {
    rpmError(RPMERR_INTERNAL,
	_("record number %u in database is bad -- skipping.\n"), dbi->dbi_lastoffset);
    if (keyp && dbi->dbi_lastoffset)
	memcpy(&mi->mi_offset, keyp, sizeof(mi->mi_offset));
    continue;
}

	    /*
	     * If we got the next key, save the header instance number.
	     * For db1 Packages (db1->dbi_lastoffset != 0), always copy.
	     * For db3 Packages, instance 0 (i.e. mi->mi_setx == 0) is the
	     * largest header instance in the database, and should be
	     * skipped.
	     */
	    if (rc == 0 && keyp && (dbi->dbi_lastoffset || mi->mi_setx))
		memcpy(&mi->mi_offset, keyp, sizeof(mi->mi_offset));

	    /* Terminate on error or end of keys */
	    if (rc || (mi->mi_setx && mi->mi_offset == 0))
		return NULL;
	}
	mi->mi_setx++;
    } while (mi->mi_offset == 0);

    if (mi->mi_prevoffset && mi->mi_offset == mi->mi_prevoffset)
	goto exit;

    /* Retrieve next header */
    if (uh == NULL) {
	rc = dbiGet(dbi, mi->mi_dbc, &keyp, &keylen, &uh, &uhlen, gflags);
	if (rc)
	    return NULL;
    }

    /* Free current header */
    if (mi->mi_h) {
	if (mi->mi_modified && mi->mi_prevoffset)
	    (void)dbiUpdateRecord(dbi, mi->mi_dbc, mi->mi_prevoffset, mi->mi_h);
	mi->mi_h = headerFree(mi->mi_h);
    }

    /* Is this the end of the iteration? */
    if (uh == NULL)
	goto exit;

    mi->mi_h = headerCopyLoad(uh);
    /* XXX db1 with hybrid, simulated db interface on falloc.c needs free. */
    if (dbi->dbi_api == 1) uh = _free(uh);

    /* Did the header load correctly? */
    if (mi->mi_h == NULL) {
	rpmError(RPMERR_BADHEADER,
		_("rpmdb: damaged header instance #%u retrieved, skipping.\n"),
		mi->mi_offset);
	goto top;
    }

    /*
     * Skip this header if iterator selector (if any) doesn't match.
     */
    if (mireSkip(mi)) {
	/* XXX hack, can't restart with Packages locked on single instance. */
	if (mi->mi_set || mi->mi_keyp == NULL)
	    goto top;
	return NULL;
    }

    mi->mi_prevoffset = mi->mi_offset;
    mi->mi_modified = 0;

exit:
#ifdef	NOTNOW
    if (mi->mi_h) {
	const char *n, *v, *r;
	(void) headerNVR(mi->mi_h, &n, &v, &r);
	rpmMessage(RPMMESS_DEBUG, "%s-%s-%s at 0x%x, h %p\n", n, v, r,
		mi->mi_offset, mi->mi_h);
    }
#endif
    /*@-retexpose -retalias@*/
    /*@-compdef -usereleased@*/ return mi->mi_h; /*@=compdef =usereleased@*/
    /*@=retexpose =retalias@*/
}

static void rpmdbSortIterator(/*@null@*/ rpmdbMatchIterator mi)
	/*@modifies mi @*/
{
    if (mi && mi->mi_set && mi->mi_set->recs && mi->mi_set->count > 0) {
	qsort(mi->mi_set->recs, mi->mi_set->count, sizeof(*mi->mi_set->recs),
		hdrNumCmp);
	mi->mi_sorted = 1;
    }
}

static int rpmdbGrowIterator(/*@null@*/ rpmdbMatchIterator mi,
		const void * keyp, size_t keylen, int fpNum)
	/*@modifies mi, fileSystem @*/
{
    dbiIndex dbi = NULL;
    DBC * dbcursor = NULL;
    dbiIndexSet set = NULL;
    int rc;
    int xx;

    if (!(mi && keyp))
	return 1;

    dbi = dbiOpen(mi->mi_rpmdb, mi->mi_rpmtag, 0);
    if (dbi == NULL)
	return 1;

    if (keylen == 0)
	keylen = strlen(keyp);

    xx = dbiCopen(dbi, &dbcursor, 0);
    rc = dbiSearch(dbi, dbcursor, keyp, keylen, &set);
    xx = dbiCclose(dbi, dbcursor, 0);
    dbcursor = NULL;

    if (rc == 0) {	/* success */
	int i;
	for (i = 0; i < set->count; i++)
	    set->recs[i].fpNum = fpNum;

	if (mi->mi_set == NULL) {
	    mi->mi_set = set;
	    set = NULL;
	} else {
	    mi->mi_set->recs = xrealloc(mi->mi_set->recs,
		(mi->mi_set->count + set->count) * sizeof(*(mi->mi_set->recs)));
	    memcpy(mi->mi_set->recs + mi->mi_set->count, set->recs,
		set->count * sizeof(*(mi->mi_set->recs)));
	    mi->mi_set->count += set->count;
	}
    }

    set = dbiFreeIndexSet(set);
    return rc;
}

int rpmdbPruneIterator(rpmdbMatchIterator mi, int * hdrNums,
	int nHdrNums, int sorted)
{
    if (mi == NULL || hdrNums == NULL || nHdrNums <= 0)
	return 1;

    if (mi->mi_set)
	(void) dbiPruneSet(mi->mi_set, hdrNums, nHdrNums, sizeof(*hdrNums), sorted);
    return 0;
}

int rpmdbAppendIterator(rpmdbMatchIterator mi, const int * hdrNums, int nHdrNums)
{
    if (mi == NULL || hdrNums == NULL || nHdrNums <= 0)
	return 1;

    if (mi->mi_set == NULL)
	mi->mi_set = xcalloc(1, sizeof(*mi->mi_set));
    (void) dbiAppendSet(mi->mi_set, hdrNums, nHdrNums, sizeof(*hdrNums), 0);
    return 0;
}

rpmdbMatchIterator rpmdbInitIterator(rpmdb rpmdb, int rpmtag,
	const void * keyp, size_t keylen)
{
    rpmdbMatchIterator mi = NULL;
    dbiIndexSet set = NULL;
    dbiIndex dbi;
    const void * mi_keyp = NULL;
    int isLabel = 0;

    if (rpmdb == NULL)
	return NULL;
    /* XXX HACK to remove rpmdbFindByLabel/findMatches from the API */
    switch (rpmtag) {
    case RPMDBI_LABEL:
	rpmtag = RPMTAG_NAME;
	isLabel = 1;
	break;
    }

    dbi = dbiOpen(rpmdb, rpmtag, 0);
    if (dbi == NULL)
	return NULL;

#if 0
    assert(dbi->dbi_rmw == NULL);	/* db3: avoid "lost" cursors */
    assert(dbi->dbi_lastoffset == 0);	/* db0: avoid "lost" cursors */
#else
if (dbi->dbi_rmw)
fprintf(stderr, "*** RMW %s %p\n", tagName(rpmtag), dbi->dbi_rmw);
#endif

    dbi->dbi_lastoffset = 0;		/* db0: rewind to beginning */

    if (rpmtag != RPMDBI_PACKAGES && keyp) {
	DBC * dbcursor = NULL;
	int rc;
	int xx;

	if (isLabel) {
	    /* XXX HACK to get rpmdbFindByLabel out of the API */
	    xx = dbiCopen(dbi, &dbcursor, 0);
	    rc = dbiFindByLabel(dbi, dbcursor, keyp, &set);
	    xx = dbiCclose(dbi, dbcursor, 0);
	    dbcursor = NULL;
	} else if (rpmtag == RPMTAG_BASENAMES) {
	    rc = rpmdbFindByFile(rpmdb, keyp, &set);
	} else {
	    xx = dbiCopen(dbi, &dbcursor, 0);
	    /*@-nullpass@*/	/* LCL: kep != NULL here. */
	    rc = dbiSearch(dbi, dbcursor, keyp, keylen, &set);
	    /*@=nullpass@*/
	    xx = dbiCclose(dbi, dbcursor, 0);
	    dbcursor = NULL;
	}
	if (rc)	{	/* error/not found */
	    set = dbiFreeIndexSet(set);
	    return NULL;
	}
    }

    if (keyp) {
	char * k;

	if (rpmtag != RPMDBI_PACKAGES && keylen == 0)
	    keylen = strlen(keyp);
	k = xmalloc(keylen + 1);
	memcpy(k, keyp, keylen);
	k[keylen] = '\0';	/* XXX for strings */
	mi_keyp = k;
    }

    mi = xcalloc(1, sizeof(*mi));
    mi->mi_keyp = mi_keyp;
    mi->mi_keylen = keylen;

    /*@-assignexpose@*/
    mi->mi_rpmdb = rpmdb;
    /*@=assignexpose@*/
    mi->mi_rpmtag = rpmtag;

    mi->mi_dbc = NULL;
    mi->mi_set = set;
    mi->mi_setx = 0;
    mi->mi_ndups = 0;
    mi->mi_h = NULL;
    mi->mi_sorted = 0;
    mi->mi_cflags = 0;
    mi->mi_modified = 0;
    mi->mi_prevoffset = 0;
    mi->mi_offset = 0;
    mi->mi_filenum = 0;
    mi->mi_fpnum = 0;
    mi->mi_dbnum = 0;
    mi->mi_nre = 0;
    mi->mi_re = NULL;
    mi->mi_version = NULL;
    mi->mi_release = NULL;
    /*@-nullret@*/
    return mi;
    /*@=nullret@*/
}

/**
 * Remove entry from database index.
 * @param dbi		index database handle
 * @param dbcursor	index database cursor
 * @param keyp		search key
 * @param keylen	search key length
 * @param rec		record to remove
 * @return		0 on success
 */
static INLINE int removeIndexEntry(dbiIndex dbi, DBC * dbcursor,
		const void * keyp, size_t keylen, dbiIndexItem rec)
	/*@modifies *dbcursor, fileSystem @*/
{
    dbiIndexSet set = NULL;
    int rc;
    
    rc = dbiSearch(dbi, dbcursor, keyp, keylen, &set);

    if (rc < 0)			/* not found */
	rc = 0;
    else if (rc > 0)		/* error */
	rc = 1;		/* error message already generated from dbindex.c */
    else {			/* success */
	/*@-mods@*/	/* a single rec is not modified */
	rc = dbiPruneSet(set, rec, 1, sizeof(*rec), 1);
	/*@=mods@*/
	if (rc == 0 && dbiUpdateIndex(dbi, dbcursor, keyp, keylen, set))
	    rc = 1;
    }

    set = dbiFreeIndexSet(set);

    return rc;
}

/* XXX install.c uninstall.c */
int rpmdbRemove(rpmdb rpmdb, int rid, unsigned int hdrNum)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    Header h;
    sigset_t signalMask;

    {	rpmdbMatchIterator mi;
	mi = rpmdbInitIterator(rpmdb, RPMDBI_PACKAGES, &hdrNum, sizeof(hdrNum));
	h = rpmdbNextIterator(mi);
	if (h)
	    h = headerLink(h);
	mi = rpmdbFreeIterator(mi);
    }

    if (h == NULL) {
	rpmError(RPMERR_DBCORRUPT, _("%s: cannot read header at 0x%x\n"),
	      "rpmdbRemove", hdrNum);
	return 1;
    }

    /* Add remove transaction id to header. */
    if (rid > 0) {
	int_32 tid = rid;
	(void) headerAddEntry(h, RPMTAG_REMOVETID, RPM_INT32_TYPE, &tid, 1);
    }

    {	const char *n, *v, *r;
	(void) headerNVR(h, &n, &v, &r);
	rpmMessage(RPMMESS_DEBUG, "  --- %10u %s-%s-%s\n", hdrNum, n, v, r);
    }

    (void) blockSignals(rpmdb, &signalMask);

    {	int dbix;
	dbiIndexItem rec = dbiIndexNewItem(hdrNum, 0);

	if (dbiTags != NULL)
	for (dbix = 0; dbix < dbiTagsMax; dbix++) {
	    dbiIndex dbi;
	    DBC * dbcursor = NULL;
	    const char *av[1];
	    const char ** rpmvals = NULL;
	    rpmTagType rpmtype = 0;
	    int rpmcnt = 0;
	    int rpmtag;
	    int xx;
	    int i;

	    dbi = NULL;
	    rpmtag = dbiTags[dbix];

	    switch (rpmtag) {
	    /* Filter out temporary databases */
	    case RPMDBI_AVAILABLE:
	    case RPMDBI_ADDED:
	    case RPMDBI_REMOVED:
	    case RPMDBI_DEPENDS:
		continue;
		/*@notreached@*/ break;
	    case RPMDBI_PACKAGES:
	      dbi = dbiOpen(rpmdb, rpmtag, 0);
	      if (dbi != NULL) {
		xx = dbiCopen(dbi, &dbcursor, DBI_WRITECURSOR);
		xx = dbiDel(dbi, dbcursor, &hdrNum, sizeof(hdrNum), 0);
		xx = dbiCclose(dbi, dbcursor, DBI_WRITECURSOR);
		dbcursor = NULL;
		if (!dbi->dbi_no_dbsync)
		    xx = dbiSync(dbi, 0);
	      }
		continue;
		/*@notreached@*/ break;
	    }
	
	    if (!hge(h, rpmtag, &rpmtype, (void **) &rpmvals, &rpmcnt))
		continue;

	  dbi = dbiOpen(rpmdb, rpmtag, 0);
	  if (dbi != NULL) {
	    xx = dbiCopen(dbi, &dbcursor, DBI_WRITECURSOR);

	    if (rpmtype == RPM_STRING_TYPE) {

		rpmMessage(RPMMESS_DEBUG, _("removing \"%s\" from %s index.\n"), 
			(const char *)rpmvals, tagName(dbi->dbi_rpmtag));

		/* XXX force uniform headerGetEntry return */
		av[0] = (const char *) rpmvals;
		rpmvals = av;
		rpmcnt = 1;
	    } else {

		rpmMessage(RPMMESS_DEBUG, _("removing %d entries from %s index.\n"), 
			rpmcnt, tagName(dbi->dbi_rpmtag));

	    }

	    for (i = 0; i < rpmcnt; i++) {
		const void * valp;
		size_t vallen;

		/* Identify value pointer and length. */
		switch (rpmtype) {
		case RPM_CHAR_TYPE:
		case RPM_INT8_TYPE:
		    vallen = sizeof(RPM_CHAR_TYPE);
		    valp = rpmvals + i;
		    break;
		case RPM_INT16_TYPE:
		    vallen = sizeof(int_16);
		    valp = rpmvals + i;
		    break;
		case RPM_INT32_TYPE:
		    vallen = sizeof(int_32);
		    valp = rpmvals + i;
		    break;
		case RPM_BIN_TYPE:
		    vallen = rpmcnt;
		    valp = rpmvals;
		    rpmcnt = 1;		/* XXX break out of loop. */
		    break;
		case RPM_STRING_TYPE:
		case RPM_I18NSTRING_TYPE:
		    rpmcnt = 1;		/* XXX break out of loop. */
		    /*@fallthrough@*/
		case RPM_STRING_ARRAY_TYPE:
		default:
		    vallen = strlen(rpmvals[i]);
		    valp = rpmvals[i];
		    break;
		}

		/*
		 * This is almost right, but, if there are duplicate tag
		 * values, there will be duplicate attempts to remove
		 * the header instance. It's easier to just ignore errors
		 * than to do things correctly.
		 */
		xx = removeIndexEntry(dbi, dbcursor, valp, vallen, rec);
	    }

	    xx = dbiCclose(dbi, dbcursor, DBI_WRITECURSOR);
	    dbcursor = NULL;

	    if (!dbi->dbi_no_dbsync)
		xx = dbiSync(dbi, 0);
	  }

	    rpmvals = hfd(rpmvals, rpmtype);
	    rpmtype = 0;
	    rpmcnt = 0;
	}

	rec = _free(rec);
    }

    (void) unblockSignals(rpmdb, &signalMask);

    h = headerFree(h);

    return 0;
}

/**
 * Add entry to database index.
 * @param dbi		index database handle
 * @param dbcursor	index database cursor
 * @param keyp		search key
 * @param keylen	search key length
 * @param rec		record to add
 * @return		0 on success
 */
static INLINE int addIndexEntry(dbiIndex dbi, DBC * dbcursor,
		const char * keyp, size_t keylen, dbiIndexItem rec)
	/*@modifies *dbcursor, fileSystem @*/
{
    dbiIndexSet set = NULL;
    int rc;

    rc = dbiSearch(dbi, dbcursor, keyp, keylen, &set);

    if (rc > 0) {		/* error */
	rc = 1;
    } else {

	/* With duplicates, cursor is positioned, discard the record. */
	if (rc == 0 && dbi->dbi_permit_dups)
	    set = dbiFreeIndexSet(set);

	if (set == NULL || rc < 0) {		/* not found */
	    rc = 0;
	    set = xcalloc(1, sizeof(*set));
	}
	(void) dbiAppendSet(set, rec, 1, sizeof(*rec), 0);
	if (dbiUpdateIndex(dbi, dbcursor, keyp, keylen, set))
	    rc = 1;
    }
    set = dbiFreeIndexSet(set);

    return 0;
}

/* XXX install.c */
int rpmdbAdd(rpmdb rpmdb, int iid, Header h)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    sigset_t signalMask;
    const char ** baseNames;
    rpmTagType bnt;
    int count = 0;
    dbiIndex dbi;
    int dbix;
    unsigned int gflags = 0;	/* dbiGet() flags */
    unsigned int pflags = 0;	/* dbiPut() flags */
    unsigned int hdrNum = 0;
    int rc = 0;
    int xx;

    if (iid > 0) {
	int_32 tid = iid;
	(void) headerRemoveEntry(h, RPMTAG_REMOVETID);
	(void) headerAddEntry(h, RPMTAG_INSTALLTID, RPM_INT32_TYPE, &tid, 1);
    }

    /*
     * If old style filename tags is requested, the basenames need to be
     * retrieved early, and the header needs to be converted before
     * being written to the package header database.
     */

    (void) hge(h, RPMTAG_BASENAMES, &bnt, (void **) &baseNames, &count);

    if (_noDirTokens)
	expandFilelist(h);

    (void) blockSignals(rpmdb, &signalMask);

    {
	unsigned int firstkey = 0;
	DBC * dbcursor = NULL;
	void * keyp = &firstkey;
	size_t keylen = sizeof(firstkey);
	void * datap = NULL;
	size_t datalen = 0;

      dbi = dbiOpen(rpmdb, RPMDBI_PACKAGES, 0);
      if (dbi != NULL) {

	/* XXX db0: hack to pass sizeof header to fadAlloc */
	datap = h;
	datalen = headerSizeof(h, HEADER_MAGIC_NO);

	xx = dbiCopen(dbi, &dbcursor, DBI_WRITECURSOR);

	/* Retrieve join key for next header instance. */

	rc = dbiGet(dbi, dbcursor, &keyp, &keylen, &datap, &datalen, gflags);

	hdrNum = 0;
	if (rc == 0 && datap)
	    memcpy(&hdrNum, datap, sizeof(hdrNum));
	++hdrNum;
	if (rc == 0 && datap) {
	    /*@-refcounttrans@*/
	    memcpy(datap, &hdrNum, sizeof(hdrNum));
	    /*@=refcounttrans@*/
	} else {
	    datap = &hdrNum;
	    datalen = sizeof(hdrNum);
	}

	rc = dbiPut(dbi, dbcursor, keyp, keylen, datap, datalen, pflags);
	xx = dbiSync(dbi, 0);

	xx = dbiCclose(dbi, dbcursor, DBI_WRITECURSOR);
	dbcursor = NULL;
      }

    }

    if (rc) {
	rpmError(RPMERR_DBCORRUPT,
		_("error(%d) allocating new package instance\n"), rc);
	goto exit;
    }

    /* Now update the indexes */

    if (hdrNum)
    {	dbiIndexItem rec = dbiIndexNewItem(hdrNum, 0);

	if (dbiTags != NULL)
	for (dbix = 0; dbix < dbiTagsMax; dbix++) {
	    DBC * dbcursor = NULL;
	    const char *av[1];
	    const char **rpmvals = NULL;
	    rpmTagType rpmtype = 0;
	    int rpmcnt = 0;
	    int rpmtag;
	    int_32 * requireFlags;
	    int i, j;

	    dbi = NULL;
	    requireFlags = NULL;
	    rpmtag = dbiTags[dbix];

	    switch (rpmtag) {
	    /* Filter out temporary databases */
	    case RPMDBI_AVAILABLE:
	    case RPMDBI_ADDED:
	    case RPMDBI_REMOVED:
	    case RPMDBI_DEPENDS:
		continue;
		/*@notreached@*/ break;
	    case RPMDBI_PACKAGES:
	      dbi = dbiOpen(rpmdb, rpmtag, 0);
	      if (dbi != NULL) {
		xx = dbiCopen(dbi, &dbcursor, DBI_WRITECURSOR);
		xx = dbiUpdateRecord(dbi, dbcursor, hdrNum, h);
		xx = dbiCclose(dbi, dbcursor, DBI_WRITECURSOR);
		dbcursor = NULL;
		if (!dbi->dbi_no_dbsync)
		    xx = dbiSync(dbi, 0);
		{   const char *n, *v, *r;
		    (void) headerNVR(h, &n, &v, &r);
		    rpmMessage(RPMMESS_DEBUG, "  +++ %10u %s-%s-%s\n", hdrNum, n, v, r);
		}
	      }
		continue;
		/*@notreached@*/ break;
	    /* XXX preserve legacy behavior */
	    case RPMTAG_BASENAMES:
		rpmtype = bnt;
		rpmvals = baseNames;
		rpmcnt = count;
		break;
	    case RPMTAG_REQUIRENAME:
		(void) hge(h, rpmtag, &rpmtype, (void **)&rpmvals, &rpmcnt);
		(void) hge(h, RPMTAG_REQUIREFLAGS, NULL, (void **)&requireFlags, NULL);
		break;
	    default:
		(void) hge(h, rpmtag, &rpmtype, (void **)&rpmvals, &rpmcnt);
		break;
	    }

	    if (rpmcnt <= 0) {
		if (rpmtag != RPMTAG_GROUP)
		    continue;

		/* XXX preserve legacy behavior */
		rpmtype = RPM_STRING_TYPE;
		rpmvals = (const char **) "Unknown";
		rpmcnt = 1;
	    }

	  dbi = dbiOpen(rpmdb, rpmtag, 0);
	  if (dbi != NULL) {

	    xx = dbiCopen(dbi, &dbcursor, DBI_WRITECURSOR);
	    if (rpmtype == RPM_STRING_TYPE) {
		rpmMessage(RPMMESS_DEBUG, _("adding \"%s\" to %s index.\n"), 
			(const char *)rpmvals, tagName(dbi->dbi_rpmtag));

		/* XXX force uniform headerGetEntry return */
		/*@-observertrans@*/
		av[0] = (const char *) rpmvals;
		/*@=observertrans@*/
		rpmvals = av;
		rpmcnt = 1;
	    } else {

		rpmMessage(RPMMESS_DEBUG, _("adding %d entries to %s index.\n"), 
			rpmcnt, tagName(dbi->dbi_rpmtag));

	    }

	    for (i = 0; i < rpmcnt; i++) {
		const void * valp;
		size_t vallen;

		/*
		 * Include the tagNum in all indices. rpm-3.0.4 and earlier
		 * included the tagNum only for files.
		 */
		switch (dbi->dbi_rpmtag) {
		case RPMTAG_REQUIRENAME:
		    /* Filter out install prerequisites. */
		    if (requireFlags && isInstallPreReq(requireFlags[i]))
			continue;
		    rec->tagNum = i;
		    break;
		case RPMTAG_TRIGGERNAME:
		    if (i) {	/* don't add duplicates */
			for (j = 0; j < i; j++) {
			    if (!strcmp(rpmvals[i], rpmvals[j]))
				/*@innerbreak@*/ break;
			}
			if (j < i)
			    continue;
		    }
		    rec->tagNum = i;
		    break;
		default:
		    rec->tagNum = i;
		    break;
		}

		/* Identify value pointer and length. */
		switch (rpmtype) {
		case RPM_CHAR_TYPE:
		case RPM_INT8_TYPE:
		    vallen = sizeof(int_8);
		    valp = rpmvals + i;
		    break;
		case RPM_INT16_TYPE:
		    vallen = sizeof(int_16);
		    valp = rpmvals + i;
		    break;
		case RPM_INT32_TYPE:
		    vallen = sizeof(int_32);
		    valp = rpmvals + i;
		    break;
		case RPM_BIN_TYPE:
		    vallen = rpmcnt;
		    valp = rpmvals;
		    rpmcnt = 1;		/* XXX break out of loop. */
		    break;
		case RPM_STRING_TYPE:
		case RPM_I18NSTRING_TYPE:
		    rpmcnt = 1;		/* XXX break out of loop. */
		    /*@fallthrough@*/
		case RPM_STRING_ARRAY_TYPE:
		default:
		    valp = rpmvals[i];
		    vallen = strlen(rpmvals[i]);
		    break;
		}

		rc += addIndexEntry(dbi, dbcursor, valp, vallen, rec);
	    }
	    xx = dbiCclose(dbi, dbcursor, DBI_WRITECURSOR);
	    dbcursor = NULL;

	    if (!dbi->dbi_no_dbsync)
		xx = dbiSync(dbi, 0);
	  }

	/*@-observertrans@*/
	    rpmvals = hfd(rpmvals, rpmtype);
	/*@=observertrans@*/
	    rpmtype = 0;
	    rpmcnt = 0;
	}

	rec = _free(rec);
    }

exit:
    (void) unblockSignals(rpmdb, &signalMask);

    return rc;
}

/* XXX transaction.c */
int rpmdbFindFpList(rpmdb rpmdb, fingerPrint * fpList, dbiIndexSet * matchList, 
		    int numItems)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    rpmdbMatchIterator mi;
    fingerPrintCache fpc;
    Header h;
    int i;

    if (rpmdb == NULL) return 0;

    mi = rpmdbInitIterator(rpmdb, RPMTAG_BASENAMES, NULL, 0);

    /* Gather all matches from the database */
    for (i = 0; i < numItems; i++) {
	(void) rpmdbGrowIterator(mi, fpList[i].baseName, 0, i);
	matchList[i] = xcalloc(1, sizeof(*(matchList[i])));
    }

    if ((i = rpmdbGetIteratorCount(mi)) == 0) {
	mi = rpmdbFreeIterator(mi);
	return 0;
    }
    fpc = fpCacheCreate(i);

    rpmdbSortIterator(mi);
    /* iterator is now sorted by (recnum, filenum) */

    /* For each set of files matched in a package ... */
    if (mi != NULL)
    while ((h = rpmdbNextIterator(mi)) != NULL) {
	const char ** dirNames;
	const char ** baseNames;
	const char ** fullBaseNames;
	rpmTagType bnt, dnt;
	int_32 * dirIndexes;
	int_32 * fullDirIndexes;
	fingerPrint * fps;
	dbiIndexItem im;
	int start;
	int num;
	int end;

	start = mi->mi_setx - 1;
	im = mi->mi_set->recs + start;

	/* Find the end of the set of matched files in this package. */
	for (end = start + 1; end < mi->mi_set->count; end++) {
	    if (im->hdrNum != mi->mi_set->recs[end].hdrNum)
		/*@innerbreak@*/ break;
	}
	num = end - start;

	/* Compute fingerprints for this header's matches */
	(void) hge(h, RPMTAG_BASENAMES, &bnt, (void **) &fullBaseNames, NULL);
	(void) hge(h, RPMTAG_DIRNAMES, &dnt, (void **) &dirNames, NULL);
	(void) hge(h, RPMTAG_DIRINDEXES, NULL, (void **) &fullDirIndexes, NULL);

	baseNames = xcalloc(num, sizeof(*baseNames));
	dirIndexes = xcalloc(num, sizeof(*dirIndexes));
	for (i = 0; i < num; i++) {
	    baseNames[i] = fullBaseNames[im[i].tagNum];
	    dirIndexes[i] = fullDirIndexes[im[i].tagNum];
	}

	fps = xcalloc(num, sizeof(*fps));
	fpLookupList(fpc, dirNames, baseNames, dirIndexes, num, fps);

	/* Add db (recnum,filenum) to list for fingerprint matches. */
	for (i = 0; i < num; i++, im++) {
	    /*@-nullpass@*/
	    if (FP_EQUAL(fps[i], fpList[im->fpNum])) {
	    /*@=nullpass@*/
		/*@-usedef@*/
		(void) dbiAppendSet(matchList[im->fpNum], im, 1, sizeof(*im), 0);
		/*@=usedef@*/
	    }
	}

	fps = _free(fps);
	dirNames = hfd(dirNames, dnt);
	fullBaseNames = hfd(fullBaseNames, bnt);
	baseNames = _free(baseNames);
	dirIndexes = _free(dirIndexes);

	mi->mi_setx = end;
    }

    mi = rpmdbFreeIterator(mi);

    fpCacheFree(fpc);

    return 0;

}

char * db1basename (int rpmtag)
{
    char * base = NULL;
    switch (rpmtag) {
    case RPMDBI_PACKAGES:	base = "packages.rpm";		break;
    case RPMTAG_NAME:		base = "nameindex.rpm";		break;
    case RPMTAG_BASENAMES:	base = "fileindex.rpm";		break;
    case RPMTAG_GROUP:		base = "groupindex.rpm";	break;
    case RPMTAG_REQUIRENAME:	base = "requiredby.rpm";	break;
    case RPMTAG_PROVIDENAME:	base = "providesindex.rpm";	break;
    case RPMTAG_CONFLICTNAME:	base = "conflictsindex.rpm";	break;
    case RPMTAG_TRIGGERNAME:	base = "triggerindex.rpm";	break;
    default:
      {	const char * tn = tagName(rpmtag);
	base = alloca( strlen(tn) + sizeof(".idx") + 1 );
	(void) stpcpy( stpcpy(base, tn), ".idx");
      }	break;
    }
    return xstrdup(base);
}

static int rpmdbRemoveDatabase(const char * rootdir,
		const char * dbpath, int _dbapi)
	/*@modifies fileSystem @*/
{ 
    int i;
    char * filename;
    int xx;

    i = strlen(dbpath);
    if (dbpath[i - 1] != '/') {
	filename = alloca(i);
	strcpy(filename, dbpath);
	filename[i] = '/';
	filename[i + 1] = '\0';
	dbpath = filename;
    }
    
    filename = alloca(strlen(rootdir) + strlen(dbpath) + 40);

    switch (_dbapi) {
    case 3:
	if (dbiTags != NULL)
	for (i = 0; i < dbiTagsMax; i++) {
	    const char * base = tagName(dbiTags[i]);
	    sprintf(filename, "%s/%s/%s", rootdir, dbpath, base);
	    (void)rpmCleanPath(filename);
	    xx = unlink(filename);
	}
	for (i = 0; i < 16; i++) {
	    sprintf(filename, "%s/%s/__db.%03d", rootdir, dbpath, i);
	    (void)rpmCleanPath(filename);
	    xx = unlink(filename);
	}
	break;
    case 2:
    case 1:
    case 0:
	if (dbiTags != NULL)
	for (i = 0; i < dbiTagsMax; i++) {
	    const char * base = db1basename(dbiTags[i]);
	    sprintf(filename, "%s/%s/%s", rootdir, dbpath, base);
	    (void)rpmCleanPath(filename);
	    xx = unlink(filename);
	    base = _free(base);
	}
	break;
    }

    sprintf(filename, "%s/%s", rootdir, dbpath);
    (void)rpmCleanPath(filename);
    xx = rmdir(filename);

    return 0;
}

static int rpmdbMoveDatabase(const char * rootdir,
		const char * olddbpath, int _olddbapi,
		const char * newdbpath, int _newdbapi)
	/*@modifies fileSystem @*/
{
    int i;
    char * ofilename, * nfilename;
    int rc = 0;
    int xx;
 
    i = strlen(olddbpath);
    if (olddbpath[i - 1] != '/') {
	ofilename = alloca(i + 2);
	strcpy(ofilename, olddbpath);
	ofilename[i] = '/';
	ofilename[i + 1] = '\0';
	olddbpath = ofilename;
    }
    
    i = strlen(newdbpath);
    if (newdbpath[i - 1] != '/') {
	nfilename = alloca(i + 2);
	strcpy(nfilename, newdbpath);
	nfilename[i] = '/';
	nfilename[i + 1] = '\0';
	newdbpath = nfilename;
    }
    
    ofilename = alloca(strlen(rootdir) + strlen(olddbpath) + 40);
    nfilename = alloca(strlen(rootdir) + strlen(newdbpath) + 40);

    switch (_olddbapi) {
    case 3:
	if (dbiTags != NULL)
	for (i = 0; i < dbiTagsMax; i++) {
	    const char * base;
	    int rpmtag;

	    /* Filter out temporary databases */
	    switch ((rpmtag = dbiTags[i])) {
	    case RPMDBI_AVAILABLE:
	    case RPMDBI_ADDED:
	    case RPMDBI_REMOVED:
	    case RPMDBI_DEPENDS:
		continue;
		/*@notreached@*/ break;
	    default:
		break;
	    }

	    base = tagName(rpmtag);
	    sprintf(ofilename, "%s/%s/%s", rootdir, olddbpath, base);
	    (void)rpmCleanPath(ofilename);
	    if (!rpmfileexists(ofilename))
		continue;
	    sprintf(nfilename, "%s/%s/%s", rootdir, newdbpath, base);
	    (void)rpmCleanPath(nfilename);
	    if ((xx = Rename(ofilename, nfilename)) != 0)
		rc = 1;
	}
	for (i = 0; i < 16; i++) {
	    sprintf(ofilename, "%s/%s/__db.%03d", rootdir, olddbpath, i);
	    (void)rpmCleanPath(ofilename);
	    if (!rpmfileexists(ofilename))
		continue;
	    sprintf(nfilename, "%s/%s/__db.%03d", rootdir, newdbpath, i);
	    (void)rpmCleanPath(nfilename);
	    if ((xx = Rename(ofilename, nfilename)) != 0)
		rc = 1;
	}
	break;
    case 2:
    case 1:
    case 0:
	if (dbiTags != NULL)
	for (i = 0; i < dbiTagsMax; i++) {
	    const char * base;
	    int rpmtag;

	    /* Filter out temporary databases */
	    switch ((rpmtag = dbiTags[i])) {
	    case RPMDBI_AVAILABLE:
	    case RPMDBI_ADDED:
	    case RPMDBI_REMOVED:
	    case RPMDBI_DEPENDS:
		continue;
		/*@notreached@*/ break;
	    default:
		break;
	    }

	    base = db1basename(rpmtag);
	    sprintf(ofilename, "%s/%s/%s", rootdir, olddbpath, base);
	    (void)rpmCleanPath(ofilename);
	    if (!rpmfileexists(ofilename))
		continue;
	    sprintf(nfilename, "%s/%s/%s", rootdir, newdbpath, base);
	    (void)rpmCleanPath(nfilename);
	    if ((xx = Rename(ofilename, nfilename)) != 0)
		rc = 1;
	    base = _free(base);
	}
	break;
    }
    if (rc || _olddbapi == _newdbapi)
	return rc;

    rc = rpmdbRemoveDatabase(rootdir, newdbpath, _newdbapi);


    /* Remove /etc/rpm/macros.db1 configuration file if db3 rebuilt. */
    if (rc == 0 && _newdbapi == 1 && _olddbapi == 3) {
	const char * mdb1 = "/etc/rpm/macros.db1";
	struct stat st;
	if (!stat(mdb1, &st) && S_ISREG(st.st_mode) && !unlink(mdb1))
	    rpmMessage(RPMMESS_DEBUG,
		_("removing %s after successful db3 rebuild.\n"), mdb1);
    }
    return rc;
}

int rpmdbRebuild(const char * rootdir)
{
    rpmdb olddb;
    const char * dbpath = NULL;
    const char * rootdbpath = NULL;
    rpmdb newdb;
    const char * newdbpath = NULL;
    const char * newrootdbpath = NULL;
    const char * tfn;
    int nocleanup = 1;
    int failed = 0;
    int removedir = 0;
    int rc = 0, xx;
    int _dbapi;
    int _dbapi_rebuild;

    if (rootdir == NULL) rootdir = "/";

    _dbapi = rpmExpandNumeric("%{_dbapi}");
    _dbapi_rebuild = rpmExpandNumeric("%{_dbapi_rebuild}");

    /*@-nullpass@*/
    tfn = rpmGetPath("%{_dbpath}", NULL);
    /*@=nullpass@*/
    if (!(tfn && tfn[0] != '%')) {
	rpmMessage(RPMMESS_DEBUG, _("no dbpath has been set"));
	rc = 1;
	goto exit;
    }
    dbpath = rootdbpath = rpmGetPath(rootdir, tfn, NULL);
    if (!(rootdir[0] == '/' && rootdir[1] == '\0'))
	dbpath += strlen(rootdir);
    tfn = _free(tfn);

    /*@-nullpass@*/
    tfn = rpmGetPath("%{_dbpath_rebuild}", NULL);
    /*@=nullpass@*/
    if (!(tfn && tfn[0] != '%' && strcmp(tfn, dbpath))) {
	char pidbuf[20];
	char *t;
	sprintf(pidbuf, "rebuilddb.%d", (int) getpid());
	t = xmalloc(strlen(dbpath) + strlen(pidbuf) + 1);
	(void)stpcpy(stpcpy(t, dbpath), pidbuf);
	tfn = _free(tfn);
	tfn = t;
	nocleanup = 0;
    }
    newdbpath = newrootdbpath = rpmGetPath(rootdir, tfn, NULL);
    if (!(rootdir[0] == '/' && rootdir[1] == '\0'))
	newdbpath += strlen(rootdir);
    tfn = _free(tfn);

    rpmMessage(RPMMESS_DEBUG, _("rebuilding database %s into %s\n"),
	rootdbpath, newrootdbpath);

    if (!access(newrootdbpath, F_OK)) {
	rpmError(RPMERR_MKDIR, _("temporary database %s already exists\n"),
	      newrootdbpath);
	rc = 1;
	goto exit;
    }

    rpmMessage(RPMMESS_DEBUG, _("creating directory %s\n"), newrootdbpath);
    if (Mkdir(newrootdbpath, 0755)) {
	rpmError(RPMERR_MKDIR, _("creating directory %s: %s\n"),
	      newrootdbpath, strerror(errno));
	rc = 1;
	goto exit;
    }
    removedir = 1;

    rpmMessage(RPMMESS_DEBUG, _("opening old database with dbapi %d\n"),
		_dbapi);
    _rebuildinprogress = 1;
    if (openDatabase(rootdir, dbpath, _dbapi, &olddb, O_RDONLY, 0644, 
		     RPMDB_FLAG_MINIMAL)) {
	rc = 1;
	goto exit;
    }
    _dbapi = olddb->db_api;
    _rebuildinprogress = 0;

    rpmMessage(RPMMESS_DEBUG, _("opening new database with dbapi %d\n"),
		_dbapi_rebuild);
    (void) rpmDefineMacro(NULL, "_rpmdb_rebuild %{nil}", -1);
    if (openDatabase(rootdir, newdbpath, _dbapi_rebuild, &newdb, O_RDWR | O_CREAT, 0644, 0)) {
	rc = 1;
	goto exit;
    }
    _dbapi_rebuild = newdb->db_api;
    
    {	Header h = NULL;
	rpmdbMatchIterator mi;
#define	_RECNUM	rpmdbGetIteratorOffset(mi)

	/* RPMDBI_PACKAGES */
	mi = rpmdbInitIterator(olddb, RPMDBI_PACKAGES, NULL, 0);
	while ((h = rpmdbNextIterator(mi)) != NULL) {

	    /* let's sanity check this record a bit, otherwise just skip it */
	    if (!(headerIsEntry(h, RPMTAG_NAME) &&
		headerIsEntry(h, RPMTAG_VERSION) &&
		headerIsEntry(h, RPMTAG_RELEASE) &&
		headerIsEntry(h, RPMTAG_BUILDTIME)))
	    {
		rpmError(RPMERR_INTERNAL,
			_("record number %u in database is bad -- skipping.\n"),
			_RECNUM);
		continue;
	    }

	    /* Filter duplicate entries ? (bug in pre rpm-3.0.4) */
	    if (_db_filter_dups || newdb->db_filter_dups) {
		const char * name, * version, * release;
		int skip = 0;

		(void) headerNVR(h, &name, &version, &release);

		/*@-shadow@*/
		{   rpmdbMatchIterator mi;
		    mi = rpmdbInitIterator(newdb, RPMTAG_NAME, name, 0);
#ifdef	DYING
		    (void) rpmdbSetIteratorVersion(mi, version);
		    (void) rpmdbSetIteratorRelease(mi, release);
#else
		    (void) rpmdbSetIteratorRE(mi, RPMTAG_VERSION, version);
		    (void) rpmdbSetIteratorRE(mi, RPMTAG_RELEASE, release);
#endif
		    while (rpmdbNextIterator(mi)) {
			skip = 1;
			/*@innerbreak@*/ break;
		    }
		    mi = rpmdbFreeIterator(mi);
		}
		/*@=shadow@*/

		if (skip)
		    continue;
	    }

	    /* Deleted entries are eliminated in legacy headers by copy. */
	    {	Header nh = (headerIsEntry(h, RPMTAG_HEADERIMAGE)
				? headerCopy(h) : NULL);
		rc = rpmdbAdd(newdb, -1, (nh ? nh : h));
		nh = headerFree(nh);
	    }

	    if (rc) {
		rpmError(RPMERR_INTERNAL,
			_("cannot add record originally at %u\n"), _RECNUM);
		failed = 1;
		break;
	    }
	}

	mi = rpmdbFreeIterator(mi);

    }

    if (!nocleanup) {
	olddb->db_remove_env = 1;
	newdb->db_remove_env = 1;
    }
    xx = rpmdbClose(olddb);
    xx = rpmdbClose(newdb);

    if (failed) {
	rpmMessage(RPMMESS_NORMAL, _("failed to rebuild database: original database "
		"remains in place\n"));

	xx = rpmdbRemoveDatabase(rootdir, newdbpath, _dbapi_rebuild);
	rc = 1;
	goto exit;
    } else if (!nocleanup) {
	if (rpmdbMoveDatabase(rootdir, newdbpath, _dbapi_rebuild, dbpath, _dbapi)) {
	    rpmMessage(RPMMESS_ERROR, _("failed to replace old database with new "
			"database!\n"));
	    rpmMessage(RPMMESS_ERROR, _("replace files in %s with files from %s "
			"to recover"), dbpath, newdbpath);
	    rc = 1;
	    goto exit;
	}
    }
    rc = 0;

exit:
    if (removedir && !(rc == 0 && nocleanup)) {
	rpmMessage(RPMMESS_DEBUG, _("removing directory %s\n"), newrootdbpath);
	if (Rmdir(newrootdbpath))
	    rpmMessage(RPMMESS_ERROR, _("failed to remove directory %s: %s\n"),
			newrootdbpath, strerror(errno));
    }
    newrootdbpath = _free(newrootdbpath);
    rootdbpath = _free(rootdbpath);

    return rc;
}
