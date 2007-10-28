/** \ingroup header
 * \file lib/formats.c
 */

#include "system.h"
#include "rpmio_internal.h"
#include <rpmlib.h>
#include "rpmmacro.h"	/* XXX for %_i18ndomains */

#include <rpmfi.h>

#include "legacy.h"
#include "manifest.h"

#include "debug.h"


/**
 * Identify type of trigger.
 * @param type		tag type
 * @param data		tag value
 * @param formatPrefix	(unused)
 * @param padding	(unused)
 * @param element	(unused)
 * @return		formatted string
 */
static char * triggertypeFormat(int32_t type, const void * data, 
		char * formatPrefix, int padding,
		int element)
{
    const int32_t * item = data;
    char * val;

    if (type != RPM_INT32_TYPE)
	val = xstrdup(_("(not a number)"));
    else if (*item & RPMSENSE_TRIGGERPREIN)
	val = xstrdup("prein");
    else if (*item & RPMSENSE_TRIGGERIN)
	val = xstrdup("in");
    else if (*item & RPMSENSE_TRIGGERUN)
	val = xstrdup("un");
    else if (*item & RPMSENSE_TRIGGERPOSTUN)
	val = xstrdup("postun");
    else
	val = xstrdup("");
    return val;
}

/**
 * Format file permissions for display.
 * @param type		tag type
 * @param data		tag value
 * @param formatPrefix
 * @param padding
 * @param element	(unused)
 * @return		formatted string
 */
static char * permsFormat(int32_t type, const void * data,
		char * formatPrefix, int padding, int element)
{
    char * val;
    char * buf;

    if (type != RPM_INT32_TYPE) {
	val = xstrdup(_("(not a number)"));
    } else {
	val = xmalloc(15 + padding);
	strcat(formatPrefix, "s");
	buf = rpmPermsString(*((int32_t *) data));
	sprintf(val, formatPrefix, buf);
	buf = _free(buf);
    }

    return val;
}

/**
 * Format file flags for display.
 * @param type		tag type
 * @param data		tag value
 * @param formatPrefix
 * @param padding
 * @param element	(unused)
 * @return		formatted string
 */
static char * fflagsFormat(int32_t type, const void * data, 
		char * formatPrefix, int padding, int element)
{
    char * val;
    char buf[15];
    int anint = *((int32_t *) data);

    if (type != RPM_INT32_TYPE) {
	val = xstrdup(_("(not a number)"));
    } else {
	buf[0] = '\0';
	if (anint & RPMFILE_DOC)
	    strcat(buf, "d");
	if (anint & RPMFILE_CONFIG)
	    strcat(buf, "c");
	if (anint & RPMFILE_SPECFILE)
	    strcat(buf, "s");
	if (anint & RPMFILE_MISSINGOK)
	    strcat(buf, "m");
	if (anint & RPMFILE_NOREPLACE)
	    strcat(buf, "n");
	if (anint & RPMFILE_GHOST)
	    strcat(buf, "g");
	if (anint & RPMFILE_LICENSE)
	    strcat(buf, "l");
	if (anint & RPMFILE_README)
	    strcat(buf, "r");

	val = xmalloc(5 + padding);
	strcat(formatPrefix, "s");
	sprintf(val, formatPrefix, buf);
    }

    return val;
}

/**
 * Wrap a pubkey in ascii armor for display.
 * @todo Permit selectable display formats (i.e. binary).
 * @param type		tag type
 * @param data		tag value
 * @param formatPrefix	(unused)
 * @param padding	(unused)
 * @param element	no. bytes of binary data
 * @return		formatted string
 */
static char * armorFormat(int32_t type, const void * data, 
		char * formatPrefix, int padding,
		int element)
{
    const char * enc;
    const unsigned char * s;
    size_t ns;
    int atype;

    switch (type) {
    case RPM_BIN_TYPE:
	s = data;
	/* XXX HACK ALERT: element field abused as no. bytes of binary data. */
	ns = element;
	atype = PGPARMOR_SIGNATURE;	/* XXX check pkt for signature */
	break;
    case RPM_STRING_TYPE:
    case RPM_STRING_ARRAY_TYPE:
	enc = data;
	if (b64decode(enc, (void **)&s, &ns))
	    return xstrdup(_("(not base64)"));
	atype = PGPARMOR_PUBKEY;	/* XXX check pkt for pubkey */
	break;
    case RPM_NULL_TYPE:
    case RPM_CHAR_TYPE:
    case RPM_INT8_TYPE:
    case RPM_INT16_TYPE:
    case RPM_INT32_TYPE:
    case RPM_I18NSTRING_TYPE:
    default:
	return xstrdup(_("(invalid type)"));
	break;
    }

    /* XXX this doesn't use padding directly, assumes enough slop in retval. */
    return pgpArmorWrap(atype, s, ns);
}

/**
 * Encode binary data in base64 for display.
 * @todo Permit selectable display formats (i.e. binary).
 * @param type		tag type
 * @param data		tag value
 * @param formatPrefix	(unused)
 * @param padding
 * @param element
 * @return		formatted string
 */
static char * base64Format(int32_t type, const void * data, 
		char * formatPrefix, int padding, int element)
{
    char * val;

    if (type != RPM_BIN_TYPE) {
	val = xstrdup(_("(not a blob)"));
    } else {
	const char * enc;
	char * t;
	int lc;
	/* XXX HACK ALERT: element field abused as no. bytes of binary data. */
	size_t ns = element;
	size_t nt = ((ns + 2) / 3) * 4;

	/* Add additional bytes necessary for eol string(s). */
	if (b64encode_chars_per_line > 0 && b64encode_eolstr != NULL) {
	    lc = (nt + b64encode_chars_per_line - 1) / b64encode_chars_per_line;
        if (((nt + b64encode_chars_per_line - 1) % b64encode_chars_per_line) != 0)
            ++lc;
	    nt += lc * strlen(b64encode_eolstr);
	}

	val = t = xmalloc(nt + padding + 1);

	*t = '\0';
	if ((enc = b64encode(data, ns)) != NULL) {
	    t = stpcpy(t, enc);
	    enc = _free(enc);
	}
    }

    return val;
}

/**
 */
static size_t xmlstrlen(const char * s)
{
    size_t len = 0;
    int c;

    while ((c = *s++) != '\0')
    {
	switch (c) {
	case '<': case '>':	len += 4;	break;
	case '&':		len += 5;	break;
	default:		len += 1;	break;
	}
    }
    return len;
}

/**
 */
static char * xmlstrcpy(char * t, const char * s)
{
    char * te = t;
    int c;

    while ((c = *s++) != '\0') {
	switch (c) {
	case '<':	te = stpcpy(te, "&lt;");	break;
	case '>':	te = stpcpy(te, "&gt;");	break;
	case '&':	te = stpcpy(te, "&amp;");	break;
	default:	*te++ = c;			break;
	}
    }
    *te = '\0';
    return t;
}

/**
 * Wrap tag data in simple header xml markup.
 * @param type		tag type
 * @param data		tag value
 * @param formatPrefix
 * @param padding
 * @param element	(unused)
 * @return		formatted string
 */
static char * xmlFormat(int32_t type, const void * data, 
		char * formatPrefix, int padding,
		int element)
{
    const char * xtag = NULL;
    size_t nb;
    char * val;
    const char * s = NULL;
    char * t, * te;
    unsigned long anint = 0;
    int xx;

    switch (type) {
    case RPM_I18NSTRING_TYPE:
    case RPM_STRING_TYPE:
	s = data;
	xtag = "string";
	break;
    case RPM_BIN_TYPE:
    {	int cpl = b64encode_chars_per_line;
	b64encode_chars_per_line = 0;
	s = base64Format(type, data, formatPrefix, padding, element);
	b64encode_chars_per_line = cpl;
	xtag = "base64";
    }	break;
    case RPM_CHAR_TYPE:
    case RPM_INT8_TYPE:
	anint = *((uint8_t *) data);
	break;
    case RPM_INT16_TYPE:
	anint = *((uint16_t *) data);
	break;
    case RPM_INT32_TYPE:
	anint = *((uint32_t *) data);
	break;
    case RPM_NULL_TYPE:
    case RPM_STRING_ARRAY_TYPE:
    default:
	return xstrdup(_("(invalid xml type)"));
	break;
    }

    if (s == NULL) {
	int tlen = 32;
	t = memset(alloca(tlen+1), 0, tlen+1);
	if (anint != 0)
	    xx = snprintf(t, tlen, "%lu", anint);
	s = t;
	xtag = "integer";
    }

    nb = xmlstrlen(s);
    if (nb == 0) {
	nb += strlen(xtag) + sizeof("\t</>");
	te = t = alloca(nb);
	te = stpcpy( stpcpy( stpcpy(te, "\t<"), xtag), "/>");
    } else {
	nb += 2 * strlen(xtag) + sizeof("\t<></>");
	te = t = alloca(nb);
	te = stpcpy( stpcpy( stpcpy(te, "\t<"), xtag), ">");
	te = xmlstrcpy(te, s);
	te += strlen(te);
	te = stpcpy( stpcpy( stpcpy(te, "</"), xtag), ">");
    }

    /* XXX s was malloc'd */
    if (!strcmp(xtag, "base64"))
	s = _free(s);

    nb += padding;
    val = xmalloc(nb+1);
    strcat(formatPrefix, "s");
    xx = snprintf(val, nb, formatPrefix, t);
    val[nb] = '\0';

    return val;
}

/**
 * Display signature fingerprint and time.
 * @param type		tag type
 * @param data		tag value
 * @param formatPrefix	(unused)
 * @param padding
 * @param element	(unused)
 * @return		formatted string
 */
static char * pgpsigFormat(int32_t type, const void * data, 
		char * formatPrefix, int padding,
		int element)
{
    char * val, * t;

    if (type != RPM_BIN_TYPE) {
	val = xstrdup(_("(not a blob)"));
    } else {
	unsigned char * pkt = (byte *) data;
	unsigned int pktlen = 0;
	unsigned int v = *pkt;
	pgpTag tag = 0;
	unsigned int plen;
	unsigned int hlen = 0;

	if (v & 0x80) {
	    if (v & 0x40) {
		tag = (v & 0x3f);
		plen = pgpLen(pkt+1, &hlen);
	    } else {
		tag = (v >> 2) & 0xf;
		plen = (1 << (v & 0x3));
		hlen = pgpGrab(pkt+1, plen);
	    }
	
	    pktlen = 1 + plen + hlen;
	}

	if (pktlen == 0 || tag != PGPTAG_SIGNATURE) {
	    val = xstrdup(_("(not an OpenPGP signature)"));
	} else {
	    pgpDig dig = pgpNewDig();
	    pgpDigParams sigp = &dig->signature;
	    size_t nb = 0;
	    const char *tempstr;

	    (void) pgpPrtPkts(pkt, pktlen, dig, 0);

	    val = NULL;
	again:
	    nb += 100;
	    val = t = xrealloc(val, nb + 1);

	    switch (sigp->pubkey_algo) {
	    case PGPPUBKEYALGO_DSA:
		t = stpcpy(t, "DSA");
		break;
	    case PGPPUBKEYALGO_RSA:
		t = stpcpy(t, "RSA");
		break;
	    default:
		(void) snprintf(t, nb - (t - val), "%d", sigp->pubkey_algo);
		t += strlen(t);
		break;
	    }
	    if (t + 5 >= val + nb)
		goto again;
	    *t++ = '/';
	    switch (sigp->hash_algo) {
	    case PGPHASHALGO_MD5:
		t = stpcpy(t, "MD5");
		break;
	    case PGPHASHALGO_SHA1:
		t = stpcpy(t, "SHA1");
		break;
	    default:
		(void) snprintf(t, nb - (t - val), "%d", sigp->hash_algo);
		t += strlen(t);
		break;
	    }
	    if (t + strlen (", ") + 1 >= val + nb)
		goto again;

	    t = stpcpy(t, ", ");

	    /* this is important if sizeof(int32_t) ! sizeof(time_t) */
	    {	time_t dateint = pgpGrab(sigp->time, sizeof(sigp->time));
		struct tm * tstruct = localtime(&dateint);
		if (tstruct)
 		    (void) strftime(t, (nb - (t - val)), "%c", tstruct);
	    }
	    t += strlen(t);
	    if (t + strlen (", Key ID ") + 1 >= val + nb)
		goto again;
	    t = stpcpy(t, ", Key ID ");
	    tempstr = pgpHexStr(sigp->signid, sizeof(sigp->signid));
	    if (t + strlen (tempstr) > val + nb)
		goto again;
	    t = stpcpy(t, tempstr);

	    dig = pgpFreeDig(dig);
	}
    }

    return val;
}

/**
 * Format dependency flags for display.
 * @param type		tag type
 * @param data		tag value
 * @param formatPrefix
 * @param padding
 * @param element	(unused)
 * @return		formatted string
 */
static char * depflagsFormat(int32_t type, const void * data, 
		char * formatPrefix, int padding, int element)
{
    char * val;
    char buf[10];
    int anint;

    if (type != RPM_INT32_TYPE) {
	val = xstrdup(_("(not a number)"));
    } else {
	anint = *((int32_t *) data);
	buf[0] = '\0';

	if (anint & RPMSENSE_LESS) 
	    strcat(buf, "<");
	if (anint & RPMSENSE_GREATER)
	    strcat(buf, ">");
	if (anint & RPMSENSE_EQUAL)
	    strcat(buf, "=");

	val = xmalloc(5 + padding);
	strcat(formatPrefix, "s");
	sprintf(val, formatPrefix, buf);
    }

    return val;
}

/**
 * Retrieve mounted file system paths.
 * @param h		header
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int fsnamesTag( Header h, int32_t * type,
		void ** data, int32_t * count,
		int * freeData)
{
    const char ** list;

    if (rpmGetFilesystemList(&list, count))
	return 1;

    if (type) *type = RPM_STRING_ARRAY_TYPE;
    if (data) *((const char ***) data) = list;
    if (freeData) *freeData = 0;

    return 0; 
}

/**
 * Retrieve install prefixes.
 * @param h		header
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int instprefixTag(Header h, rpmTagType * type,
		const void ** data,
		int32_t * count,
		int * freeData)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    rpmTagType ipt;
    char ** array;

    if (hge(h, RPMTAG_INSTALLPREFIX, type, (void **)data, count)) {
	if (freeData) *freeData = 0;
	return 0;
    } else if (hge(h, RPMTAG_INSTPREFIXES, &ipt, (void **) &array, count)) {
	if (type) *type = RPM_STRING_TYPE;
	if (data) *data = xstrdup(array[0]);
	if (freeData) *freeData = 1;
	array = hfd(array, ipt);
	return 0;
    }

    return 1;
}

/**
 * Retrieve mounted file system space.
 * @param h		header
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int fssizesTag(Header h, rpmTagType * type,
		const void ** data, int32_t * count,
		int * freeData)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    const char ** filenames;
    int32_t * filesizes;
    uint32_t * usages;
    int numFiles;

    if (!hge(h, RPMTAG_FILESIZES, NULL, (void **) &filesizes, &numFiles)) {
	filesizes = NULL;
	numFiles = 0;
	filenames = NULL;
    } else {
	rpmfiBuildFNames(h, RPMTAG_BASENAMES, &filenames, &numFiles);
    }

    if (rpmGetFilesystemList(NULL, count))
	return 1;

    *type = RPM_INT32_TYPE;
    *freeData = 1;

    if (filenames == NULL) {
	usages = xcalloc((*count), sizeof(usages));
	*data = usages;

	return 0;
    }

    if (rpmGetFilesystemUsage(filenames, filesizes, numFiles, &usages, 0))	
	return 1;

    *data = usages;

    filenames = _free(filenames);

    return 0;
}

/**
 * Retrieve trigger info.
 * @param h		header
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int triggercondsTag(Header h, rpmTagType * type,
		const void ** data, int32_t * count,
		int * freeData)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    rpmTagType tnt, tvt, tst;
    int32_t * indices, * flags;
    char ** names, ** versions;
    int numNames, numScripts;
    char ** conds, ** s;
    char * item, * flagsStr;
    char * chptr;
    int i, j, xx;
    char buf[5];

    if (!hge(h, RPMTAG_TRIGGERNAME, &tnt, (void **) &names, &numNames)) {
	*freeData = 0;
	return 0;
    }

    xx = hge(h, RPMTAG_TRIGGERINDEX, NULL, (void **) &indices, NULL);
    xx = hge(h, RPMTAG_TRIGGERFLAGS, NULL, (void **) &flags, NULL);
    xx = hge(h, RPMTAG_TRIGGERVERSION, &tvt, (void **) &versions, NULL);
    xx = hge(h, RPMTAG_TRIGGERSCRIPTS, &tst, (void **) &s, &numScripts);
    s = hfd(s, tst);

    *freeData = 1;
    *data = conds = xmalloc(sizeof(*conds) * numScripts);
    *count = numScripts;
    *type = RPM_STRING_ARRAY_TYPE;
    for (i = 0; i < numScripts; i++) {
	chptr = xstrdup("");

	for (j = 0; j < numNames; j++) {
	    if (indices[j] != i)
		continue;

	    item = xmalloc(strlen(names[j]) + strlen(versions[j]) + 20);
	    if (flags[j] & RPMSENSE_SENSEMASK) {
		buf[0] = '%', buf[1] = '\0';
		flagsStr = depflagsFormat(RPM_INT32_TYPE, flags, buf, 0, j);
		sprintf(item, "%s %s %s", names[j], flagsStr, versions[j]);
		flagsStr = _free(flagsStr);
	    } else {
		strcpy(item, names[j]);
	    }

	    chptr = xrealloc(chptr, strlen(chptr) + strlen(item) + 5);
	    if (*chptr != '\0') strcat(chptr, ", ");
	    strcat(chptr, item);
	    item = _free(item);
	}

	conds[i] = chptr;
    }

    names = hfd(names, tnt);
    versions = hfd(versions, tvt);

    return 0;
}

/**
 * Retrieve trigger type info.
 * @param h		header
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int triggertypeTag(Header h, rpmTagType * type,
		const void ** data, int32_t * count,
		int * freeData)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    rpmTagType tst;
    int32_t * indices, * flags;
    const char ** conds;
    const char ** s;
    int i, j, xx;
    int numScripts, numNames;

    if (!hge(h, RPMTAG_TRIGGERINDEX, NULL, (void **) &indices, &numNames)) {
	*freeData = 0;
	return 1;
    }

    xx = hge(h, RPMTAG_TRIGGERFLAGS, NULL, (void **) &flags, NULL);
    xx = hge(h, RPMTAG_TRIGGERSCRIPTS, &tst, (void **) &s, &numScripts);
    s = hfd(s, tst);

    *freeData = 1;
    *data = conds = xmalloc(sizeof(*conds) * numScripts);
    *count = numScripts;
    *type = RPM_STRING_ARRAY_TYPE;
    for (i = 0; i < numScripts; i++) {
	for (j = 0; j < numNames; j++) {
	    if (indices[j] != i)
		continue;

	    if (flags[j] & RPMSENSE_TRIGGERPREIN)
		conds[i] = xstrdup("prein");
	    else if (flags[j] & RPMSENSE_TRIGGERIN)
		conds[i] = xstrdup("in");
	    else if (flags[j] & RPMSENSE_TRIGGERUN)
		conds[i] = xstrdup("un");
	    else if (flags[j] & RPMSENSE_TRIGGERPOSTUN)
		conds[i] = xstrdup("postun");
	    else
		conds[i] = xstrdup("");
	    break;
	}
    }

    return 0;
}

/**
 * Retrieve file paths.
 * @param h		header
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int filenamesTag(Header h, rpmTagType * type,
		const void ** data, int32_t * count,
		int * freeData)
{
    *type = RPM_STRING_ARRAY_TYPE;
    rpmfiBuildFNames(h, RPMTAG_BASENAMES, (const char ***) data, count);
    *freeData = 1;
    return 0; 
}

/**
 * Retrieve file classes.
 * @param h		header
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int fileclassTag(Header h, rpmTagType * type,
		const void ** data, int32_t * count,
		int * freeData)
{
    *type = RPM_STRING_ARRAY_TYPE;
    rpmfiBuildFClasses(h, (const char ***) data, count);
    *freeData = 1;
    return 0; 
}

/**
 * Retrieve file provides.
 * @param h		header
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int fileprovideTag(Header h, rpmTagType * type,
		const void ** data, int32_t * count,
		int * freeData)
{
    *type = RPM_STRING_ARRAY_TYPE;
    rpmfiBuildFDeps(h, RPMTAG_PROVIDENAME, (const char ***) data, count);
    *freeData = 1;
    return 0; 
}

/**
 * Retrieve file requires.
 * @param h		header
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int filerequireTag(Header h, rpmTagType * type,
		const void ** data, int32_t * count,
		int * freeData)
{
    *type = RPM_STRING_ARRAY_TYPE;
    rpmfiBuildFDeps(h, RPMTAG_REQUIRENAME, (const char ***) data, count);
    *freeData = 1;
    return 0; 
}

/* I18N look aside diversions */

#if defined(ENABLE_NLS)
extern int _nl_msg_cat_cntr;	/* XXX GNU gettext voodoo */
#endif
static const char * language = "LANGUAGE";

static const char * _macro_i18ndomains = "%{?_i18ndomains}";

/**
 * Retrieve i18n text.
 * @param h		header
 * @param tag		tag
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int i18nTag(Header h, int32_t tag, rpmTagType * type,
		const void ** data, int32_t * count,
		int * freeData)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    char * dstring = rpmExpand(_macro_i18ndomains, NULL);
    int rc;

    *type = RPM_STRING_TYPE;
    *data = NULL;
    *count = 0;
    *freeData = 0;

    if (dstring && *dstring) {
	char *domain, *de;
	const char * langval;
	const char * msgkey;
	const char * msgid;

	{   const char * tn = rpmTagGetName(tag);
	    const char * n;
	    char * mk;
	    size_t nb = sizeof("()");
	    int xx;
	    xx = headerNVR(h, &n, NULL, NULL);
	    if (tn)	nb += strlen(tn);
	    if (n)	nb += strlen(n);
	    mk = alloca(nb);
	    sprintf(mk, "%s(%s)", n, tn);
	    msgkey = mk;
	}

	/* change to en_US for msgkey -> msgid resolution */
	langval = getenv(language);
	(void) setenv(language, "en_US", 1);
#if defined(ENABLE_NLS)
        ++_nl_msg_cat_cntr;
#endif

	msgid = NULL;
	for (domain = dstring; domain != NULL; domain = de) {
	    de = strchr(domain, ':');
	    if (de) *de++ = '\0';
	    msgid = dgettext(domain, msgkey);
	    if (msgid != msgkey) break;
	}

	/* restore previous environment for msgid -> msgstr resolution */
	if (langval)
	    (void) setenv(language, langval, 1);
	else
	    unsetenv(language);
#if defined(ENABLE_NLS)
        ++_nl_msg_cat_cntr;
#endif

	if (domain && msgid) {
	    *data = dgettext(domain, msgid);
	    *data = xstrdup(*data);	/* XXX xstrdup has side effects. */
	    *count = 1;
	    *freeData = 1;
	}
	dstring = _free(dstring);
	if (*data)
	    return 0;
    }

    dstring = _free(dstring);

    rc = hge(h, tag, type, (void **)data, count);

    if (rc && (*data) != NULL) {
	*data = xstrdup(*data);
	*freeData = 1;
	return 0;
    }

    *freeData = 0;
    *data = NULL;
    *count = 0;
    return 1;
}

/**
 * Retrieve summary text.
 * @param h		header
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int summaryTag(Header h, rpmTagType * type,
		const void ** data, int32_t * count,
		int * freeData)
{
    return i18nTag(h, RPMTAG_SUMMARY, type, data, count, freeData);
}

/**
 * Retrieve description text.
 * @param h		header
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int descriptionTag(Header h, rpmTagType * type,
		const void ** data, int32_t * count,
		int * freeData)
{
    return i18nTag(h, RPMTAG_DESCRIPTION, type, data, count, freeData);
}

/**
 * Retrieve group text.
 * @param h		header
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int groupTag(Header h, rpmTagType * type,
		const void ** data, int32_t * count,
		int * freeData)
{
    return i18nTag(h, RPMTAG_GROUP, type, data, count, freeData);
}

/* FIX: cast? */
const struct headerSprintfExtension_s rpmHeaderFormats[] = {
    { HEADER_EXT_TAG, "RPMTAG_GROUP",		{ groupTag } },
    { HEADER_EXT_TAG, "RPMTAG_DESCRIPTION",	{ descriptionTag } },
    { HEADER_EXT_TAG, "RPMTAG_SUMMARY",		{ summaryTag } },
    { HEADER_EXT_TAG, "RPMTAG_FILECLASS",	{ fileclassTag } },
    { HEADER_EXT_TAG, "RPMTAG_FILENAMES",	{ filenamesTag } },
    { HEADER_EXT_TAG, "RPMTAG_FILEPROVIDE",	{ fileprovideTag } },
    { HEADER_EXT_TAG, "RPMTAG_FILEREQUIRE",	{ filerequireTag } },
    { HEADER_EXT_TAG, "RPMTAG_FSNAMES",		{ fsnamesTag } },
    { HEADER_EXT_TAG, "RPMTAG_FSSIZES",		{ fssizesTag } },
    { HEADER_EXT_TAG, "RPMTAG_INSTALLPREFIX",	{ instprefixTag } },
    { HEADER_EXT_TAG, "RPMTAG_TRIGGERCONDS",	{ triggercondsTag } },
    { HEADER_EXT_TAG, "RPMTAG_TRIGGERTYPE",	{ triggertypeTag } },
    { HEADER_EXT_FORMAT, "armor",		{ armorFormat } },
    { HEADER_EXT_FORMAT, "base64",		{ base64Format } },
    { HEADER_EXT_FORMAT, "pgpsig",		{ pgpsigFormat } },
    { HEADER_EXT_FORMAT, "depflags",		{ depflagsFormat } },
    { HEADER_EXT_FORMAT, "fflags",		{ fflagsFormat } },
    { HEADER_EXT_FORMAT, "perms",		{ permsFormat } },
    { HEADER_EXT_FORMAT, "permissions",		{ permsFormat } },
    { HEADER_EXT_FORMAT, "triggertype",		{ triggertypeFormat } },
    { HEADER_EXT_FORMAT, "xml",			{ xmlFormat } },
    { HEADER_EXT_MORE, NULL,		{ (void *) headerDefaultFormats } }
} ;
