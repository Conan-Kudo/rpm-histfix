/** \ingroup header
 * \file lib/package.c
 */

#include "system.h"

#include <netinet/in.h>

#include "rpmio/digest.h"
#include "rpmio/rpmio_internal.h"	/* fd*Digest(), fd stats */
#include <rpm/rpmlib.h>

#include <rpm/rpmts.h>

#include "lib/legacy.h"	/* XXX legacyRetrofit() */
#include "lib/rpmlead.h"

#include <rpm/rpmlog.h>
#include "rpmdb/header_internal.h"	/* XXX headerCheck */
#include "lib/signature.h"
#include "debug.h"

#define	alloca_strdup(_s)	strcpy(alloca(strlen(_s)+1), (_s))

static int _print_pkts = 0;

static unsigned int nkeyids_max = 256;
static unsigned int nkeyids = 0;
static unsigned int nextkeyid  = 0;
static unsigned int * keyids;

static unsigned char header_magic[8] = {
        0x8e, 0xad, 0xe8, 0x01, 0x00, 0x00, 0x00, 0x00
};

/**
 * Alignment needs (and sizeof scalars types) for internal rpm data types.
 */
static int typeAlign[16] =  {
    1,	/*!< RPM_NULL_TYPE */
    1,	/*!< RPM_CHAR_TYPE */
    1,	/*!< RPM_INT8_TYPE */
    2,	/*!< RPM_INT16_TYPE */
    4,	/*!< RPM_INT32_TYPE */
    8,	/*!< RPM_INT64_TYPE */
    1,	/*!< RPM_STRING_TYPE */
    1,	/*!< RPM_BIN_TYPE */
    1,	/*!< RPM_STRING_ARRAY_TYPE */
    1,	/*!< RPM_I18NSTRING_TYPE */
    0,
    0,
    0,
    0,
    0,
    0
};

/**
 * Sanity check on no. of tags.
 * This check imposes a limit of 65K tags, more than enough.
 */
#define hdrchkTags(_ntags)      ((_ntags) & 0xffff0000)

/**
 * Sanity check on type values.
 */
#define hdrchkType(_type) ((_type) < RPM_MIN_TYPE || (_type) > RPM_MAX_TYPE)

/**
 * Sanity check on data size and/or offset and/or count.
 * This check imposes a limit of 16 MB, more than enough.
 */
#define hdrchkData(_nbytes) ((_nbytes) & 0xff000000)

/**
 * Sanity check on data alignment for data type.
 */
#define hdrchkAlign(_type, _off)	((_off) & (typeAlign[_type]-1))

/**
 * Sanity check on range of data offset.
 */
#define hdrchkRange(_dl, _off)		((_off) < 0 || (_off) > (_dl))

void headerMergeLegacySigs(Header h, const Header sigh)
{
    HFD_t hfd = (HFD_t) headerFreeData;
    HAE_t hae = (HAE_t) headerAddEntry;
    HeaderIterator hi;
    rpm_tagtype_t type;
    rpm_tag_t tag;
    rpm_count_t count;
    const void * ptr;
    int xx;

    for (hi = headerInitIterator(sigh);
        headerNextIterator(hi, &tag, &type, &ptr, &count);
        ptr = hfd(ptr, type))
    {
	switch (tag) {
	/* XXX Translate legacy signature tag values. */
	case RPMSIGTAG_SIZE:
	    tag = RPMTAG_SIGSIZE;
	    break;
	case RPMSIGTAG_LEMD5_1:
	    tag = RPMTAG_SIGLEMD5_1;
	    break;
	case RPMSIGTAG_PGP:
	    tag = RPMTAG_SIGPGP;
	    break;
	case RPMSIGTAG_LEMD5_2:
	    tag = RPMTAG_SIGLEMD5_2;
	    break;
	case RPMSIGTAG_MD5:
	    tag = RPMTAG_SIGMD5;
	    break;
	case RPMSIGTAG_GPG:
	    tag = RPMTAG_SIGGPG;
	    break;
	case RPMSIGTAG_PGP5:
	    tag = RPMTAG_SIGPGP5;
	    break;
	case RPMSIGTAG_PAYLOADSIZE:
	    tag = RPMTAG_ARCHIVESIZE;
	    break;
	case RPMSIGTAG_SHA1:
	case RPMSIGTAG_DSA:
	case RPMSIGTAG_RSA:
	default:
	    if (!(tag >= HEADER_SIGBASE && tag < HEADER_TAGBASE))
		continue;
	    break;
	}
	if (ptr == NULL) continue;	/* XXX can't happen */
	if (!headerIsEntry(h, tag)) {
	    if (hdrchkType(type))
		continue;
	    if (count < 0 || hdrchkData(count))
		continue;
	    switch(type) {
	    case RPM_NULL_TYPE:
		continue;
		break;
	    case RPM_CHAR_TYPE:
	    case RPM_INT8_TYPE:
	    case RPM_INT16_TYPE:
	    case RPM_INT32_TYPE:
		if (count != 1)
		    continue;
		break;
	    case RPM_STRING_TYPE:
	    case RPM_BIN_TYPE:
		if (count >= 16*1024)
		    continue;
		break;
	    case RPM_STRING_ARRAY_TYPE:
	    case RPM_I18NSTRING_TYPE:
		continue;
		break;
	    }
 	    xx = hae(h, tag, type, ptr, count);
	}
    }
    hi = headerFreeIterator(hi);
}

Header headerRegenSigHeader(const Header h, int noArchiveSize)
{
    HFD_t hfd = (HFD_t) headerFreeData;
    Header sigh = rpmNewSignature();
    HeaderIterator hi;
    rpm_count_t count;
    rpm_tag_t tag, stag;
    rpm_tagtype_t type;
    const void * ptr;
    int xx;

    for (hi = headerInitIterator(h);
        headerNextIterator(hi, &tag, &type, &ptr, &count);
        ptr = hfd(ptr, type))
    {
	switch (tag) {
	/* XXX Translate legacy signature tag values. */
	case RPMTAG_SIGSIZE:
	    stag = RPMSIGTAG_SIZE;
	    break;
	case RPMTAG_SIGLEMD5_1:
	    stag = RPMSIGTAG_LEMD5_1;
	    break;
	case RPMTAG_SIGPGP:
	    stag = RPMSIGTAG_PGP;
	    break;
	case RPMTAG_SIGLEMD5_2:
	    stag = RPMSIGTAG_LEMD5_2;
	    break;
	case RPMTAG_SIGMD5:
	    stag = RPMSIGTAG_MD5;
	    break;
	case RPMTAG_SIGGPG:
	    stag = RPMSIGTAG_GPG;
	    break;
	case RPMTAG_SIGPGP5:
	    stag = RPMSIGTAG_PGP5;
	    break;
	case RPMTAG_ARCHIVESIZE:
	    /* XXX rpm-4.1 and later has archive size in signature header. */
	    if (noArchiveSize)
		continue;
	    stag = RPMSIGTAG_PAYLOADSIZE;
	    break;
	case RPMTAG_SHA1HEADER:
	case RPMTAG_DSAHEADER:
	case RPMTAG_RSAHEADER:
	default:
	    if (!(tag >= HEADER_SIGBASE && tag < HEADER_TAGBASE))
		continue;
	    stag = tag;
	    break;
	}
	if (ptr == NULL) continue;	/* XXX can't happen */
	if (!headerIsEntry(sigh, stag))
	    xx = headerAddEntry(sigh, stag, type, ptr, count);
    }
    hi = headerFreeIterator(hi);
    return sigh;
}

/**
 * Remember current key id.
 * @param ts		transaction set
 * @return		0 if new keyid, otherwise 1
 */
static int rpmtsStashKeyid(rpmts ts)
{
    const void * sig = rpmtsSig(ts);
    pgpDig dig = rpmtsDig(ts);
    pgpDigParams sigp = rpmtsSignature(ts);
    unsigned int keyid;
    int i;

    if (sig == NULL || dig == NULL || sigp == NULL)
	return 0;

    keyid = pgpGrab(sigp->signid+4, 4);
    if (keyid == 0)
	return 0;

    if (keyids != NULL)
    for (i = 0; i < nkeyids; i++) {
	if (keyid == keyids[i])
	    return 1;
    }

    if (nkeyids < nkeyids_max) {
	nkeyids++;
	keyids = xrealloc(keyids, nkeyids * sizeof(*keyids));
    }
    if (keyids)		/* XXX can't happen */
	keyids[nextkeyid] = keyid;
    nextkeyid++;
    nextkeyid %= nkeyids_max;

    return 0;
}

int headerVerifyInfo(int il, int dl, const void * pev, void * iv, int negate)
{
    entryInfo pe = (entryInfo) pev;
    entryInfo info = iv;
    int i;

    for (i = 0; i < il; i++) {
	info->tag = ntohl(pe[i].tag);
	info->type = ntohl(pe[i].type);
	info->offset = ntohl(pe[i].offset);
	if (negate)
	    info->offset = -info->offset;
	info->count = ntohl(pe[i].count);

	if (hdrchkType(info->type))
	    return i;
	if (hdrchkAlign(info->type, info->offset))
	    return i;
	if (!negate && hdrchkRange(dl, info->offset))
	    return i;
	if (hdrchkData(info->count))
	    return i;

    }
    return -1;
}

/**
 * Check header consistency, performing headerGetEntry() the hard way.
 *
 * Sanity checks on the header are performed while looking for a
 * header-only digest or signature to verify the blob. If found,
 * the digest or signature is verified.
 *
 * @param ts		transaction set
 * @param uh		unloaded header blob
 * @param uc		no. of bytes in blob (or 0 to disable)
 * @retval *msg		signature verification msg
 * @return		RPMRC_OK/RPMRC_NOTFOUND/RPMRC_FAIL
 */
rpmRC headerCheck(rpmts ts, const void * uh, size_t uc, const char ** msg)
{
    pgpDig dig;
    char buf[8*BUFSIZ];
    int32_t * ei = (int32_t *) uh;
    int32_t il = ntohl(ei[0]);
    int32_t dl = ntohl(ei[1]);
    entryInfo pe = (entryInfo) &ei[2];
    int32_t ildl[2];
    int32_t pvlen = sizeof(ildl) + (il * sizeof(*pe)) + dl;
    unsigned char * dataStart = (unsigned char *) (pe + il);
    indexEntry entry = memset(alloca(sizeof(*entry)), 0, sizeof(*entry));
    entryInfo info = memset(alloca(sizeof(*info)), 0, sizeof(*info));
    const void * sig = NULL;
    unsigned const char * b;
    rpmVSFlags vsflags = rpmtsVSFlags(ts);
    size_t siglen = 0;
    size_t blen;
    size_t nb;
    int32_t ril = 0;
    unsigned char * regionEnd = NULL;
    rpmRC rc = RPMRC_FAIL;	/* assume failure */
    int xx;
    int i;
    static int hclvl;

    hclvl++;
    buf[0] = '\0';

    /* Is the blob the right size? */
    if (uc > 0 && pvlen != uc) {
	(void) snprintf(buf, sizeof(buf),
		_("blob size(%d): BAD, 8 + 16 * il(%d) + dl(%d)\n"),
		(int)uc, (int)il, (int)dl);
	goto exit;
    }

    /* Check (and convert) the 1st tag element. */
    xx = headerVerifyInfo(1, dl, pe, &entry->info, 0);
    if (xx != -1) {
	(void) snprintf(buf, sizeof(buf),
		_("tag[%d]: BAD, tag %d type %d offset %d count %d\n"),
		0, entry->info.tag, entry->info.type,
		entry->info.offset, entry->info.count);
	goto exit;
    }

    /* Is there an immutable header region tag? */
    if (!(entry->info.tag == RPMTAG_HEADERIMMUTABLE
       && entry->info.type == RPM_BIN_TYPE
       && entry->info.count == REGION_TAG_COUNT))
    {
	rc = RPMRC_NOTFOUND;
	goto exit;
    }

    /* Is the offset within the data area? */
    if (entry->info.offset >= dl) {
	(void) snprintf(buf, sizeof(buf),
		_("region offset: BAD, tag %d type %d offset %d count %d\n"),
		entry->info.tag, entry->info.type,
		entry->info.offset, entry->info.count);
	goto exit;
    }

    /* Is there an immutable header region tag trailer? */
    regionEnd = dataStart + entry->info.offset;
    (void) memcpy(info, regionEnd, REGION_TAG_COUNT);
    regionEnd += REGION_TAG_COUNT;

    xx = headerVerifyInfo(1, dl, info, &entry->info, 1);
    if (xx != -1 ||
	!(entry->info.tag == RPMTAG_HEADERIMMUTABLE
       && entry->info.type == RPM_BIN_TYPE
       && entry->info.count == REGION_TAG_COUNT))
    {
	(void) snprintf(buf, sizeof(buf),
		_("region trailer: BAD, tag %d type %d offset %d count %d\n"),
		entry->info.tag, entry->info.type,
		entry->info.offset, entry->info.count);
	goto exit;
    }
    memset(info, 0, sizeof(*info));

    /* Is the no. of tags in the region less than the total no. of tags? */
    ril = entry->info.offset/sizeof(*pe);
    if ((entry->info.offset % sizeof(*pe)) || ril > il) {
	(void) snprintf(buf, sizeof(buf),
		_("region size: BAD, ril(%d) > il(%d)\n"), ril, il);
	goto exit;
    }

    /* Find a header-only digest/signature tag. */
    for (i = ril; i < il; i++) {
	xx = headerVerifyInfo(1, dl, pe+i, &entry->info, 0);
	if (xx != -1) {
	    (void) snprintf(buf, sizeof(buf),
		_("tag[%d]: BAD, tag %d type %d offset %d count %d\n"),
		i, entry->info.tag, entry->info.type,
		entry->info.offset, entry->info.count);
	    goto exit;
	}

	switch (entry->info.tag) {
	case RPMTAG_SHA1HEADER:
	    if (vsflags & RPMVSF_NOSHA1HEADER)
		break;
	    blen = 0;
	    for (b = dataStart + entry->info.offset; *b != '\0'; b++) {
		if (strchr("0123456789abcdefABCDEF", *b) == NULL)
		    break;
		blen++;
	    }
	    if (entry->info.type != RPM_STRING_TYPE || *b != '\0' || blen != 40)
	    {
		(void) snprintf(buf, sizeof(buf), _("hdr SHA1: BAD, not hex\n"));
		goto exit;
	    }
	    if (info->tag == 0) {
		*info = entry->info;	/* structure assignment */
		siglen = blen + 1;
	    }
	    break;
	case RPMTAG_RSAHEADER:
	    if (vsflags & RPMVSF_NORSAHEADER)
		break;
	    if (entry->info.type != RPM_BIN_TYPE) {
		(void) snprintf(buf, sizeof(buf), _("hdr RSA: BAD, not binary\n"));
		goto exit;
	    }
	    *info = entry->info;	/* structure assignment */
	    siglen = info->count;
	    break;
	case RPMTAG_DSAHEADER:
	    if (vsflags & RPMVSF_NODSAHEADER)
		break;
	    if (entry->info.type != RPM_BIN_TYPE) {
		(void) snprintf(buf, sizeof(buf), _("hdr DSA: BAD, not binary\n"));
		goto exit;
	    }
	    *info = entry->info;	/* structure assignment */
	    siglen = info->count;
	    break;
	default:
	    break;
	}
    }
    rc = RPMRC_NOTFOUND;

exit:
    /* Return determined RPMRC_OK/RPMRC_FAIL conditions. */
    if (rc != RPMRC_NOTFOUND) {
	buf[sizeof(buf)-1] = '\0';
	if (msg) *msg = xstrdup(buf);
	hclvl--;
	return rc;
    }

    /* If no header-only digest/signature, then do simple sanity check. */
    if (info->tag == 0) {
verifyinfo_exit:
	xx = headerVerifyInfo(ril-1, dl, pe+1, &entry->info, 0);
	if (xx != -1) {
	    (void) snprintf(buf, sizeof(buf),
		_("tag[%d]: BAD, tag %d type %d offset %d count %d\n"),
		xx+1, entry->info.tag, entry->info.type,
		entry->info.offset, entry->info.count);
	    rc = RPMRC_FAIL;
	} else {
	    (void) snprintf(buf, sizeof(buf), "Header sanity check: OK\n");
	    rc = RPMRC_OK;
	}
	buf[sizeof(buf)-1] = '\0';
	if (msg) *msg = xstrdup(buf);
	hclvl--;
	return rc;
    }

    /* Verify header-only digest/signature. */
    dig = rpmtsDig(ts);
    if (dig == NULL)
	goto verifyinfo_exit;
    dig->nbytes = 0;

    sig = memcpy(xmalloc(siglen), dataStart + info->offset, siglen);
    (void) rpmtsSetSig(ts, info->tag, info->type, sig, (size_t) info->count);

    switch (info->tag) {
    case RPMTAG_RSAHEADER:
	/* Parse the parameters from the OpenPGP packets that will be needed. */
	xx = pgpPrtPkts(sig, info->count, dig, (_print_pkts & rpmIsDebug()));
	if (dig->signature.version != 3 && dig->signature.version != 4) {
	    rpmlog(RPMLOG_ERR,
		_("skipping header with unverifiable V%u signature\n"),
		dig->signature.version);
	    rpmtsCleanDig(ts);
	    rc = RPMRC_FAIL;
	    goto exit;
	}

	ildl[0] = htonl(ril);
	ildl[1] = (regionEnd - dataStart);
	ildl[1] = htonl(ildl[1]);

	(void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_DIGEST), 0);
	dig->hdrmd5ctx = rpmDigestInit(dig->signature.hash_algo, RPMDIGEST_NONE);

	b = (unsigned char *) header_magic;
	nb = sizeof(header_magic);
        (void) rpmDigestUpdate(dig->hdrmd5ctx, b, nb);
        dig->nbytes += nb;

	b = (unsigned char *) ildl;
	nb = sizeof(ildl);
        (void) rpmDigestUpdate(dig->hdrmd5ctx, b, nb);
        dig->nbytes += nb;

	b = (unsigned char *) pe;
	nb = (htonl(ildl[0]) * sizeof(*pe));
        (void) rpmDigestUpdate(dig->hdrmd5ctx, b, nb);
        dig->nbytes += nb;

	b = (unsigned char *) dataStart;
	nb = htonl(ildl[1]);
        (void) rpmDigestUpdate(dig->hdrmd5ctx, b, nb);
        dig->nbytes += nb;
	(void) rpmswExit(rpmtsOp(ts, RPMTS_OP_DIGEST), dig->nbytes);

	break;
    case RPMTAG_DSAHEADER:
	/* Parse the parameters from the OpenPGP packets that will be needed. */
	xx = pgpPrtPkts(sig, info->count, dig, (_print_pkts & rpmIsDebug()));
	if (dig->signature.version != 3 && dig->signature.version != 4) {
	    rpmlog(RPMLOG_ERR,
		_("skipping header with unverifiable V%u signature\n"),
		dig->signature.version);
	    rpmtsCleanDig(ts);
	    rc = RPMRC_FAIL;
	    goto exit;
	}
    case RPMTAG_SHA1HEADER:
	ildl[0] = htonl(ril);
	ildl[1] = (regionEnd - dataStart);
	ildl[1] = htonl(ildl[1]);

	(void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_DIGEST), 0);
	dig->hdrsha1ctx = rpmDigestInit(PGPHASHALGO_SHA1, RPMDIGEST_NONE);

	b = (unsigned char *) header_magic;
	nb = sizeof(header_magic);
        (void) rpmDigestUpdate(dig->hdrsha1ctx, b, nb);
        dig->nbytes += nb;

	b = (unsigned char *) ildl;
	nb = sizeof(ildl);
        (void) rpmDigestUpdate(dig->hdrsha1ctx, b, nb);
        dig->nbytes += nb;

	b = (unsigned char *) pe;
	nb = (htonl(ildl[0]) * sizeof(*pe));
        (void) rpmDigestUpdate(dig->hdrsha1ctx, b, nb);
        dig->nbytes += nb;

	b = (unsigned char *) dataStart;
	nb = htonl(ildl[1]);
        (void) rpmDigestUpdate(dig->hdrsha1ctx, b, nb);
        dig->nbytes += nb;
	(void) rpmswExit(rpmtsOp(ts, RPMTS_OP_DIGEST), dig->nbytes);

	break;
    default:
	sig = _free(sig);
	break;
    }

    buf[0] = '\0';
    rc = rpmVerifySignature(ts, buf);

    buf[sizeof(buf)-1] = '\0';
    if (msg) *msg = xstrdup(buf);

    /* XXX headerCheck can recurse, free info only at top level. */
    if (hclvl == 1)
	rpmtsCleanDig(ts);
    if (info->tag == RPMTAG_SHA1HEADER)
	sig = _free(sig);
    hclvl--;
    return rc;
}

rpmRC rpmReadHeader(rpmts ts, FD_t fd, Header *hdrp, const char ** msg)
{
    char buf[BUFSIZ];
    int32_t block[4];
    int32_t il;
    int32_t dl;
    int32_t * ei = NULL;
    size_t uc;
    int32_t nb;
    Header h = NULL;
    rpmRC rc = RPMRC_FAIL;		/* assume failure */
    int xx;

    buf[0] = '\0';

    if (hdrp)
	*hdrp = NULL;
    if (msg)
	*msg = NULL;

    memset(block, 0, sizeof(block));
    if ((xx = timedRead(fd, (char *)block, sizeof(block))) != sizeof(block)) {
	(void) snprintf(buf, sizeof(buf),
		_("hdr size(%d): BAD, read returned %d\n"), (int)sizeof(block), xx);
	goto exit;
    }
    if (memcmp(block, header_magic, sizeof(header_magic))) {
	(void) snprintf(buf, sizeof(buf), _("hdr magic: BAD\n"));
	goto exit;
    }
    il = ntohl(block[2]);
    if (hdrchkTags(il)) {
	(void) snprintf(buf, sizeof(buf),
		_("hdr tags: BAD, no. of tags(%d) out of range\n"), il);

	goto exit;
    }
    dl = ntohl(block[3]);
    if (hdrchkData(dl)) {
	(void) snprintf(buf, sizeof(buf),
		_("hdr data: BAD, no. of bytes(%d) out of range\n"), dl);
	goto exit;
    }

    nb = (il * sizeof(struct entryInfo_s)) + dl;
    uc = sizeof(il) + sizeof(dl) + nb;
    ei = xmalloc(uc);
    ei[0] = block[2];
    ei[1] = block[3];
    if ((xx = timedRead(fd, (char *)&ei[2], nb)) != nb) {
	(void) snprintf(buf, sizeof(buf),
		_("hdr blob(%d): BAD, read returned %d\n"), nb, xx);
	goto exit;
    }

    /* Sanity check header tags */
    rc = headerCheck(ts, ei, uc, msg);
    if (rc != RPMRC_OK)
	goto exit;

    /* OK, blob looks sane, load the header. */
    h = headerLoad(ei);
    if (h == NULL) {
	(void) snprintf(buf, sizeof(buf), _("hdr load: BAD\n"));
        goto exit;
    }
    h->flags |= HEADERFLAG_ALLOCATED;
    ei = NULL;	/* XXX will be freed with header */
    
exit:
    if (hdrp && h && rc == RPMRC_OK)
	*hdrp = headerLink(h);
    ei = _free(ei);
    h = headerFree(h);

    if (msg != NULL && *msg == NULL && buf[0] != '\0') {
	buf[sizeof(buf)-1] = '\0';
	*msg = xstrdup(buf);
    }

    return rc;
}

rpmRC rpmReadPackageFile(rpmts ts, FD_t fd, const char * fn, Header * hdrp)
{
    pgpDig dig;
    char buf[8*BUFSIZ];
    ssize_t count;
    rpmlead l = NULL;
    Header sigh = NULL;
    rpm_tag_t sigtag;
    rpm_tagtype_t sigtype;
    const void * sig;
    rpm_count_t siglen;
    rpmtsOpX opx;
    size_t nb;
    Header h = NULL;
    const char * msg;
    rpmVSFlags vsflags;
    rpmRC rc = RPMRC_FAIL;	/* assume failure */
    int xx;

    if (hdrp) *hdrp = NULL;

    l = rpmLeadNew();

    if ((rc = rpmLeadRead(fd, l)) == RPMRC_OK) {
	rc = rpmLeadCheck(l, fn);
    }
    l = rpmLeadFree(l);

    if (rc != RPMRC_OK)
	goto exit;

    /* Read the signature header. */
    msg = NULL;
    rc = rpmReadSignature(fd, &sigh, RPMSIGTYPE_HEADERSIG, &msg);
    switch (rc) {
    default:
	rpmlog(RPMLOG_ERR, _("%s: rpmReadSignature failed: %s"), fn,
		(msg && *msg ? msg : "\n"));
	msg = _free(msg);
	goto exit;
	break;
    case RPMRC_OK:
	if (sigh == NULL) {
	    rpmlog(RPMLOG_ERR, _("%s: No signature available\n"), fn);
	    rc = RPMRC_FAIL;
	    goto exit;
	}
	break;
    }
    msg = _free(msg);

#define	_chk(_mask)	(sigtag == 0 && !(vsflags & (_mask)))

    /*
     * Figger the most effective available signature.
     * Prefer signatures over digests, then header-only over header+payload.
     * DSA will be preferred over RSA if both exist because tested first.
     * Note that NEEDPAYLOAD prevents header+payload signatures and digests.
     */
    sigtag = 0;
    opx = 0;
    vsflags = rpmtsVSFlags(ts);
    if (_chk(RPMVSF_NODSAHEADER) && headerIsEntry(sigh, RPMSIGTAG_DSA)) {
	sigtag = RPMSIGTAG_DSA;
    } else
    if (_chk(RPMVSF_NORSAHEADER) && headerIsEntry(sigh, RPMSIGTAG_RSA)) {
	sigtag = RPMSIGTAG_RSA;
    } else
    if (_chk(RPMVSF_NODSA|RPMVSF_NEEDPAYLOAD) &&
	headerIsEntry(sigh, RPMSIGTAG_GPG))
    {
	sigtag = RPMSIGTAG_GPG;
	fdInitDigest(fd, PGPHASHALGO_SHA1, 0);
	opx = RPMTS_OP_SIGNATURE;
    } else
    if (_chk(RPMVSF_NORSA|RPMVSF_NEEDPAYLOAD) &&
	headerIsEntry(sigh, RPMSIGTAG_PGP))
    {
	sigtag = RPMSIGTAG_PGP;
	fdInitDigest(fd, PGPHASHALGO_MD5, 0);
	opx = RPMTS_OP_SIGNATURE;
    } else
    if (_chk(RPMVSF_NOSHA1HEADER) && headerIsEntry(sigh, RPMSIGTAG_SHA1)) {
	sigtag = RPMSIGTAG_SHA1;
    } else
    if (_chk(RPMVSF_NOMD5|RPMVSF_NEEDPAYLOAD) &&
	headerIsEntry(sigh, RPMSIGTAG_MD5))
    {
	sigtag = RPMSIGTAG_MD5;
	fdInitDigest(fd, PGPHASHALGO_MD5, 0);
	opx = RPMTS_OP_DIGEST;
    }

    /* Read the metadata, computing digest(s) on the fly. */
    h = NULL;
    msg = NULL;

    /* XXX stats will include header i/o and setup overhead. */
    /* XXX repackaged packages have appended tags, legacy dig/sig check fails */
    if (opx > 0)
	(void) rpmswEnter(rpmtsOp(ts, opx), 0);
    nb = -fd->stats->ops[FDSTAT_READ].bytes;
    rc = rpmReadHeader(ts, fd, &h, &msg);
    nb += fd->stats->ops[FDSTAT_READ].bytes;
    if (opx > 0)
	(void) rpmswExit(rpmtsOp(ts, opx), nb);

    if (rc != RPMRC_OK || h == NULL) {
	rpmlog(RPMLOG_ERR, _("%s: headerRead failed: %s"), fn,
		(msg && *msg ? msg : "\n"));
	msg = _free(msg);
	goto exit;
    }
    msg = _free(msg);

    /* Any digests or signatures to check? */
    if (sigtag == 0) {
	rc = RPMRC_OK;
	goto exit;
    }

    dig = rpmtsDig(ts);
    if (dig == NULL) {
	rc = RPMRC_FAIL;
	goto exit;
    }
    dig->nbytes = 0;

    /* Retrieve the tag parameters from the signature header. */
    sig = NULL;
    xx = headerGetEntry(sigh, sigtag, &sigtype, (void **) &sig, &siglen);
    if (sig == NULL) {
	rc = RPMRC_FAIL;
	goto exit;
    }
    (void) rpmtsSetSig(ts, sigtag, sigtype, sig, siglen);

    switch (sigtag) {
    case RPMSIGTAG_RSA:
	/* Parse the parameters from the OpenPGP packets that will be needed. */
	xx = pgpPrtPkts(sig, siglen, dig, (_print_pkts & rpmIsDebug()));
	if (dig->signature.version != 3 && dig->signature.version != 4) {
	    rpmlog(RPMLOG_ERR,
		_("skipping package %s with unverifiable V%u signature\n"),
		fn, dig->signature.version);
	    rc = RPMRC_FAIL;
	    goto exit;
	}
    {	void * uh = NULL;
	rpm_tagtype_t uht;
	rpm_count_t uhc;

	if (!headerGetEntry(h, RPMTAG_HEADERIMMUTABLE, &uht, &uh, &uhc))
	    break;
	(void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_DIGEST), 0);
	dig->hdrmd5ctx = rpmDigestInit(dig->signature.hash_algo, RPMDIGEST_NONE);
	(void) rpmDigestUpdate(dig->hdrmd5ctx, header_magic, sizeof(header_magic));
	dig->nbytes += sizeof(header_magic);
	(void) rpmDigestUpdate(dig->hdrmd5ctx, uh, uhc);
	dig->nbytes += uhc;
	(void) rpmswExit(rpmtsOp(ts, RPMTS_OP_DIGEST), dig->nbytes);
	rpmtsOp(ts, RPMTS_OP_DIGEST)->count--;	/* XXX one too many */
	uh = headerFreeData(uh, uht);
    }	break;
    case RPMSIGTAG_DSA:
	/* Parse the parameters from the OpenPGP packets that will be needed. */
	xx = pgpPrtPkts(sig, siglen, dig, (_print_pkts & rpmIsDebug()));
	if (dig->signature.version != 3 && dig->signature.version != 4) {
	    rpmlog(RPMLOG_ERR,
		_("skipping package %s with unverifiable V%u signature\n"), 
		fn, dig->signature.version);
	    rc = RPMRC_FAIL;
	    goto exit;
	}
    case RPMSIGTAG_SHA1:
    {	void * uh = NULL;
	rpm_tagtype_t uht;
	rpm_count_t uhc;

	if (!headerGetEntry(h, RPMTAG_HEADERIMMUTABLE, &uht, &uh, &uhc))
	    break;
	(void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_DIGEST), 0);
	dig->hdrsha1ctx = rpmDigestInit(PGPHASHALGO_SHA1, RPMDIGEST_NONE);
	(void) rpmDigestUpdate(dig->hdrsha1ctx, header_magic, sizeof(header_magic));
	dig->nbytes += sizeof(header_magic);
	(void) rpmDigestUpdate(dig->hdrsha1ctx, uh, uhc);
	dig->nbytes += uhc;
	(void) rpmswExit(rpmtsOp(ts, RPMTS_OP_DIGEST), dig->nbytes);
	if (sigtag == RPMSIGTAG_SHA1)
	    rpmtsOp(ts, RPMTS_OP_DIGEST)->count--;	/* XXX one too many */
	uh = headerFreeData(uh, uht);
    }	break;
    case RPMSIGTAG_GPG:
    case RPMSIGTAG_PGP5:	/* XXX legacy */
    case RPMSIGTAG_PGP:
	/* Parse the parameters from the OpenPGP packets that will be needed. */
	xx = pgpPrtPkts(sig, siglen, dig, (_print_pkts & rpmIsDebug()));

	if (dig->signature.version != 3 && dig->signature.version != 4) {
	    rpmlog(RPMLOG_ERR,
		_("skipping package %s with unverifiable V%u signature\n"),
		fn, dig->signature.version);
	    rc = RPMRC_FAIL;
	    goto exit;
	}
    case RPMSIGTAG_MD5:
	/* Legacy signatures need the compressed payload in the digest too. */
	(void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_DIGEST), 0);
	while ((count = Fread(buf, sizeof(buf[0]), sizeof(buf), fd)) > 0)
	    dig->nbytes += count;
	(void) rpmswExit(rpmtsOp(ts, RPMTS_OP_DIGEST), dig->nbytes);
	rpmtsOp(ts, RPMTS_OP_DIGEST)->count--;	/* XXX one too many */
	dig->nbytes += nb;	/* XXX include size of header blob. */
	if (count < 0) {
	    rpmlog(RPMLOG_ERR, _("%s: Fread failed: %s\n"),
					fn, Fstrerror(fd));
	    rc = RPMRC_FAIL;
	    goto exit;
	}

	fdStealDigest(fd, dig);
	break;
    }

/** @todo Implement disable/enable/warn/error/anal policy. */

    buf[0] = '\0';
    rc = rpmVerifySignature(ts, buf);
    switch (rc) {
    case RPMRC_OK:		/* Signature is OK. */
	rpmlog(RPMLOG_DEBUG, "%s: %s", fn, buf);
	break;
    case RPMRC_NOTTRUSTED:	/* Signature is OK, but key is not trusted. */
    case RPMRC_NOKEY:		/* Public key is unavailable. */
	/* XXX Print NOKEY/NOTTRUSTED warning only once. */
    {	int lvl = (rpmtsStashKeyid(ts) ? RPMLOG_DEBUG : RPMLOG_WARNING);
	rpmlog(lvl, "%s: %s", fn, buf);
    }	break;
    case RPMRC_NOTFOUND:	/* Signature is unknown type. */
	rpmlog(RPMLOG_WARNING, "%s: %s", fn, buf);
	break;
    default:
    case RPMRC_FAIL:		/* Signature does not verify. */
	rpmlog(RPMLOG_ERR, "%s: %s", fn, buf);
	break;
    }

exit:
    if (rc != RPMRC_FAIL && h != NULL && hdrp != NULL) {
	/* 
         * Convert legacy headers on the fly. Not having "new" style compressed
         * filenames is close enough estimate for legacy indication... 
         */
	if (!headerIsEntry(h, RPMTAG_DIRNAMES)) {
	    legacyRetrofit(h);
	}
	
	/* Append (and remap) signature tags to the metadata. */
	headerMergeLegacySigs(h, sigh);

	/* Bump reference count for return. */
	*hdrp = headerLink(h);
    }
    h = headerFree(h);
    rpmtsCleanDig(ts);
    sigh = rpmFreeSignature(sigh);
    return rc;
}

/**
 * Check for supported payload format in header.
 * @param h		header to check
 * @return		RPMRC_OK if supported, RPMRC_FAIL otherwise
 */
rpmRC headerCheckPayloadFormat(Header h) {
    rpmRC rc = RPMRC_FAIL;
    int xx;
    const char *payloadfmt = NULL;

    xx = headerGetEntry(h, RPMTAG_PAYLOADFORMAT, NULL, 
			(void **)&payloadfmt, NULL);
    /* 
     * XXX Ugh, rpm 3.x packages don't have payload format tag. Instead
     * of blinly allowing, should check somehow (HDRID existence or... ?)
     */
    if (!payloadfmt)
	return RPMRC_OK;

    if (payloadfmt && strncmp(payloadfmt, "cpio", strlen("cpio")) == 0) {
	rc = RPMRC_OK;
    } else {
        char *nevra = headerGetNEVRA(h, NULL);
        if (payloadfmt && strncmp(payloadfmt, "drpm", strlen("drpm")) == 0) {
            rpmlog(RPMLOG_ERR,
                     _("%s is a Delta RPM and cannot be directly installed\n"),
                     nevra);
        } else {
            rpmlog(RPMLOG_ERR, 
                     _("Unsupported payload (%s) in package %s\n"),
                     payloadfmt ? payloadfmt : "none", nevra);
        } 
        nevra = _free(nevra);
    }
    return rc;
}


