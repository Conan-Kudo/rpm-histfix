/**
 * \file rpmdb/tagname.c
 */

#include "system.h"

#include <rpm/rpmtag.h>
#include <rpm/rpmstring.h>
#include "debug.h"

struct headerTagIndices_s {
    int (*loadIndex) (headerTagTableEntry ** ipp, int * np,
                int (*cmp) (const void * avp, const void * bvp));
                                        /*!< load sorted tag index. */
    headerTagTableEntry * byName;	/*!< header tags sorted by name. */
    int byNameSize;			/*!< no. of entries. */
    int (*byNameCmp) (const void * avp, const void * bvp);				/*!< compare entries by name. */
    rpm_tag_t (*tagValue) (const char * name);	/* return value from name. */
    headerTagTableEntry * byValue;	/*!< header tags sorted by value. */
    int byValueSize;			/*!< no. of entries. */
    int (*byValueCmp) (const void * avp, const void * bvp);				/*!< compare entries by value. */
    const char * (*tagName) (rpm_tag_t value);	/* Return name from value. */
    rpm_tagtype_t (*tagType) (rpm_tag_t value);	/* Return type from value. */
};

/**
 * Compare tag table entries by name.
 * @param *avp		tag table entry a
 * @param *bvp		tag table entry b
 * @return		comparison
 */
static int tagCmpName(const void * avp, const void * bvp)
{
    headerTagTableEntry a = *(const headerTagTableEntry *) avp;
    headerTagTableEntry b = *(const headerTagTableEntry *) bvp;
    return strcmp(a->name, b->name);
}

/**
 * Compare tag table entries by value.
 * @param *avp		tag table entry a
 * @param *bvp		tag table entry b
 * @return		comparison
 */
static int tagCmpValue(const void * avp, const void * bvp)
{
    headerTagTableEntry a = *(const headerTagTableEntry *) avp;
    headerTagTableEntry b = *(const headerTagTableEntry *) bvp;
    int ret = (a->val - b->val);
    /* Make sure that sort is stable, longest name first. */
    if (ret == 0)
	ret = (strlen(b->name) - strlen(a->name));
    return ret;
}

/**
 * Load/sort a tag index.
 * @retval *ipp		tag index
 * @retval *np		no. of tags
 * @param cmp		sort compare routine
 * @return		0 always
 */
static int tagLoadIndex(headerTagTableEntry ** ipp, int * np,
		int (*cmp) (const void * avp, const void * bvp))
{
    headerTagTableEntry tte, *ip;
    int n = 0;

    ip = xcalloc(rpmTagTableSize, sizeof(*ip));
    n = 0;
    for (tte = (headerTagTableEntry)rpmTagTable; tte->name != NULL; tte++) {
	ip[n] = tte;
	n++;
    }
assert(n == rpmTagTableSize);

    if (n > 1)
	qsort(ip, n, sizeof(*ip), cmp);
    *ipp = ip;
    *np = n;
    return 0;
}


/* forward refs */
static const char * _tagName(rpm_tag_t tag);
static rpm_tagtype_t _tagType(rpm_tag_t tag);
static rpm_tag_t _tagValue(const char * tagstr);

static struct headerTagIndices_s _rpmTags = {
    tagLoadIndex,
    NULL, 0, tagCmpName, _tagValue,
    NULL, 0, tagCmpValue, _tagName, _tagType,
};

headerTagIndices rpmTags = &_rpmTags;

static const char * _tagName(rpm_tag_t tag)
{
    static char nameBuf[128];	/* XXX yuk */
    const struct headerTagTableEntry_s *t;
    int comparison, i, l, u;
    int xx;
    char *s;

    if (_rpmTags.byValue == NULL)
	xx = tagLoadIndex(&_rpmTags.byValue, &_rpmTags.byValueSize, tagCmpValue);

    switch (tag) {
    case RPMDBI_PACKAGES:
	strcpy(nameBuf, "Packages");
	break;
    case RPMDBI_DEPENDS:
	strcpy(nameBuf, "Depends");
	break;
    case RPMDBI_ADDED:
	strcpy(nameBuf, "Added");
	break;
    case RPMDBI_REMOVED:
	strcpy(nameBuf, "Removed");
	break;
    case RPMDBI_AVAILABLE:
	strcpy(nameBuf, "Available");
	break;
    case RPMDBI_HDLIST:
	strcpy(nameBuf, "Hdlist");
	break;
    case RPMDBI_ARGLIST:
	strcpy(nameBuf, "Arglist");
	break;
    case RPMDBI_FTSWALK:
	strcpy(nameBuf, "Ftswalk");
	break;

    /* XXX make sure rpmdb indices are identically named. */
    case RPMTAG_CONFLICTS:
	strcpy(nameBuf, "Conflictname");
	break;
    case RPMTAG_HDRID:
	strcpy(nameBuf, "Sha1header");
	break;

    default:
	strcpy(nameBuf, "(unknown)");
	if (_rpmTags.byValue == NULL)
	    break;
	l = 0;
	u = _rpmTags.byValueSize;
	while (l < u) {
	    i = (l + u) / 2;
	    t = _rpmTags.byValue[i];
	
	    comparison = (tag - t->val);

	    if (comparison < 0)
		u = i;
	    else if (comparison > 0)
		l = i + 1;
	    else {
		nameBuf[0] = nameBuf[1] = '\0';
		/* Make sure that the bsearch retrieve is stable. */
		while (i > 0 && tag == _rpmTags.byValue[i-1]->val) {
		    i--;
		}
		t = _rpmTags.byValue[i];
		if (t->name != NULL)
		    strcpy(nameBuf, t->name + (sizeof("RPMTAG_")-1));
		for (s = nameBuf+1; *s != '\0'; s++)
		    *s = xtolower(*s);
		break;
	    }
	}
	break;
    }
    return nameBuf;
}

static rpm_tagtype_t _tagType(rpm_tag_t tag)
{
    const struct headerTagTableEntry_s *t;
    int comparison, i, l, u;
    int xx;

    if (_rpmTags.byValue == NULL)
	xx = tagLoadIndex(&_rpmTags.byValue, &_rpmTags.byValueSize, tagCmpValue);

    switch (tag) {
    case RPMDBI_PACKAGES:
    case RPMDBI_DEPENDS:
    case RPMDBI_ADDED:
    case RPMDBI_REMOVED:
    case RPMDBI_AVAILABLE:
    case RPMDBI_HDLIST:
    case RPMDBI_ARGLIST:
    case RPMDBI_FTSWALK:
	break;
    default:
	if (_rpmTags.byValue == NULL)
	    break;
	l = 0;
	u = _rpmTags.byValueSize;
	while (l < u) {
	    i = (l + u) / 2;
	    t = _rpmTags.byValue[i];
	
	    comparison = (tag - t->val);

	    if (comparison < 0)
		u = i;
	    else if (comparison > 0)
		l = i + 1;
	    else {
		/* Make sure that the bsearch retrieve is stable. */
		while (i > 0 && t->val == _rpmTags.byValue[i-1]->val) {
		    i--;
		}
		t = _rpmTags.byValue[i];
		return t->type;
	    }
	}
	break;
    }
    return RPM_NULL_TYPE;
}

static rpm_tag_t _tagValue(const char * tagstr)
{
    const struct headerTagTableEntry_s *t;
    int comparison, i, l, u;
    int xx;

    if (!xstrcasecmp(tagstr, "Packages"))
	return RPMDBI_PACKAGES;
    if (!xstrcasecmp(tagstr, "Depends"))
	return RPMDBI_DEPENDS;
    if (!xstrcasecmp(tagstr, "Added"))
	return RPMDBI_ADDED;
    if (!xstrcasecmp(tagstr, "Removed"))
	return RPMDBI_REMOVED;
    if (!xstrcasecmp(tagstr, "Available"))
	return RPMDBI_AVAILABLE;
    if (!xstrcasecmp(tagstr, "Hdlist"))
	return RPMDBI_HDLIST;
    if (!xstrcasecmp(tagstr, "Arglist"))
	return RPMDBI_ARGLIST;
    if (!xstrcasecmp(tagstr, "Ftswalk"))
	return RPMDBI_FTSWALK;

    if (_rpmTags.byName == NULL)
	xx = tagLoadIndex(&_rpmTags.byName, &_rpmTags.byNameSize, tagCmpName);
    if (_rpmTags.byName == NULL)
	return RPMTAG_NOT_FOUND;

    l = 0;
    u = _rpmTags.byNameSize;
    while (l < u) {
	i = (l + u) / 2;
	t = _rpmTags.byName[i];
	
	comparison = xstrcasecmp(tagstr, t->name + (sizeof("RPMTAG_")-1));

	if (comparison < 0)
	    u = i;
	else if (comparison > 0)
	    l = i + 1;
	else
	    return t->val;
    }
    return RPMTAG_NOT_FOUND;
}

const char * rpmTagGetName(rpm_tag_t tag)
{
    return ((*rpmTags->tagName)(tag));
}

/**
 * Return tag data type from value.
 * @param tag           tag value
 * @return              tag data type, RPM_NULL_TYPE on not found.
 */
rpm_tagtype_t rpmTagGetType(rpm_tag_t tag)
{
    return ((*rpmTags->tagType)(tag));
}

/**
 * Return tag value from name.
 * @param tagstr        name of tag
 * @return              tag value, RPMTAG_NOT_FOUND on not found
 */
rpm_tag_t rpmTagGetValue(const char * tagstr)
{
    return ((*rpmTags->tagValue)(tagstr));
}

