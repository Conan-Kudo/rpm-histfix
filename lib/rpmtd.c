#include "system.h"

#include <rpm/rpmtd.h>
#include <rpm/rpmstring.h>
#include <rpm/rpmpgp.h>

#include "debug.h"

typedef char * (*headerTagFormatFunction) (rpmtd td, char *formatPrefix);

extern void *rpmHeaderFormatFuncByValue(rpmtdFormats fmt);

rpmtd rpmtdNew(void)
{
    rpmtd td = xmalloc(sizeof(*td));
    rpmtdReset(td);
    return td;
}

rpmtd rpmtdFree(rpmtd td)
{
    /* permit free on NULL td */
    if (td != NULL) {
	/* XXX should we free data too - a flag maybe? */
	free(td);
    }
    return NULL;
}

void rpmtdReset(rpmtd td)
{
    assert(td != NULL);

    memset(td, 0, sizeof(*td));
    td->ix = -1;
}

void rpmtdFreeData(rpmtd td)
{
    assert(td != NULL);

    if (td->flags & RPMTD_ALLOCED) {
	if (td->flags & RPMTD_PTR_ALLOCED) {
	    assert(td->data != NULL);
	    char **data = td->data;
	    for (int i = 0; i < td->count; i++) {
		free(data[i]);
	    }
	}
	free(td->data);
    }
    rpmtdReset(td);
}

rpm_count_t rpmtdCount(rpmtd td)
{
    assert(td != NULL);
    /* fix up for binary type abusing count as data length */
    return (td->type == RPM_BIN_TYPE) ? 1 : td->count;
}

rpmTag rpmtdTag(rpmtd td)
{
    assert(td != NULL);
    return td->tag;
}

rpmTagType rpmtdType(rpmtd td)
{
    assert(td != NULL);
    return td->type;
}

int rpmtdGetIndex(rpmtd td)
{
    assert(td != NULL);
    return td->ix;
}

int rpmtdSetIndex(rpmtd td, int index)
{
    assert(td != NULL);

    if (index < 0 || index >= rpmtdCount(td)) {
	return -1;
    }
    td->ix = index;
    return td->ix;
}

int rpmtdInit(rpmtd td)
{
    assert(td != NULL);

    /* XXX check that this is an array type? */
    td->ix = -1;
    return 0;
}

int rpmtdNext(rpmtd td)
{
    assert(td != NULL);

    int i = -1;
    
    if (++td->ix >= 0) {
	if (td->ix < rpmtdCount(td)) {
	    i = td->ix;
	} else {
	    td->ix = i;
	}
    }
    return i;
}

uint32_t *rpmtdNextUint32(rpmtd td)
{
    assert(td != NULL);
    uint32_t *res = NULL;
    if (rpmtdNext(td) >= 0) {
	res = rpmtdGetUint32(td);
    }
    return res;
}

const char *rpmtdNextString(rpmtd td)
{
    assert(td != NULL);
    const char *res = NULL;
    if (rpmtdNext(td) >= 0) {
	res = rpmtdGetString(td);
    }
    return res;
}

char * rpmtdGetChar(rpmtd td)
{
    char *res = NULL;

    assert(td != NULL);

    if (td->type == RPM_CHAR_TYPE) {
	int ix = (td->ix >= 0 ? td->ix : 0);
	res = (char *) td->data + ix;
    } 
    return res;
}
uint16_t * rpmtdGetUint16(rpmtd td)
{
    uint16_t *res = NULL;

    assert(td != NULL);

    if (td->type == RPM_INT16_TYPE) {
	int ix = (td->ix >= 0 ? td->ix : 0);
	res = (uint16_t *) td->data + ix;
    } 
    return res;
}

uint32_t * rpmtdGetUint32(rpmtd td)
{
    uint32_t *res = NULL;

    assert(td != NULL);

    if (td->type == RPM_INT32_TYPE) {
	int ix = (td->ix >= 0 ? td->ix : 0);
	res = (uint32_t *) td->data + ix;
    } 
    return res;
}
const char * rpmtdGetString(rpmtd td)
{
    const char *str = NULL;

    assert(td != NULL);

    if (td->type == RPM_STRING_TYPE) {
	str = (const char *) td->data;
    } else if (td->type == RPM_STRING_ARRAY_TYPE ||
	       td->type == RPM_I18NSTRING_TYPE) {
	/* XXX TODO: check for array bounds */
	int ix = (td->ix >= 0 ? td->ix : 0);
	str = *((const char**) td->data + ix);
    } 
    return str;
}

char *rpmtdFormat(rpmtd td, rpmtdFormats fmt, const char *errmsg)
{
    headerTagFormatFunction func = rpmHeaderFormatFuncByValue(fmt);
    const char *err = NULL;
    char *str = NULL;

    if (func) {
	char fmtbuf[50]; /* yuck, get rid of this */
	strcpy(fmtbuf, "%");
	str = func(td, fmtbuf);
    } else {
	err = _("Unknown format");
    }
    
    if (err && errmsg) {
	errmsg = err;
    }

    return str;
}

int rpmtdSetTag(rpmtd td, rpmTag tag)
{
    assert(td != NULL);
    rpmTagType newtype = rpmTagGetType(tag);
    int rc = 0;

    /* 
     * Sanity checks: 
     * - is the new tag valid at all
     * - if changing tag of non-empty container, require matching type 
     */
    if (newtype == RPM_NULL_TYPE)
	goto exit;

    if (td->data || td->count > 0) {
	if (rpmTagGetType(td->tag) != rpmTagGetType(tag)) {
	    goto exit;
	}
    } 

    td->tag = tag;
    td->type = newtype & RPM_MASK_TYPE;
    rc = 1;
    
exit:
    return rc;
}

int rpmtdFromArgv(rpmtd td, rpmTag tag, ARGV_t argv)
{
    int count = argvCount(argv);
    rpmTagType type = rpmTagGetType(tag) & RPM_MASK_TYPE;

    if (type != RPM_STRING_ARRAY_TYPE || count < 1)
	return 0;

    assert(td != NULL);
    rpmtdReset(td);
    td->type = type;
    td->tag = tag;
    td->count = count;
    td->data = argv;
    return 1;
}

int rpmtdFromArgi(rpmtd td, rpmTag tag, ARGI_t argi)
{
    int count = argiCount(argi);
    rpmTagType type = rpmTagGetType(tag) & RPM_MASK_TYPE;
    rpmTagReturnType retype = rpmTagGetType(tag) & RPM_MASK_RETURN_TYPE;

    if (type != RPM_INT32_TYPE || retype != RPM_ARRAY_RETURN_TYPE || count < 1)
	return 0;

    assert(td != NULL);
    rpmtdReset(td);
    td->type = type;
    td->tag = tag;
    td->count = count;
    td->data = argiData(argi);
    return 1;
}

rpmtd rpmtdDup(rpmtd td)
{
    rpmtd newtd = NULL;
    char **data = NULL;
    int i;
    
    assert(td != NULL);
    /* TODO: permit other types too */
    if (td->type != RPM_STRING_ARRAY_TYPE && td->type != RPM_I18NSTRING_TYPE) {
	return NULL;
    }

    /* deep-copy container and data, drop immutable flag */
    newtd = rpmtdNew();
    memcpy(newtd, td, sizeof(*td));
    newtd->flags &= ~(RPMTD_IMMUTABLE);

    newtd->flags |= (RPMTD_ALLOCED | RPMTD_PTR_ALLOCED);
    newtd->data = data = xmalloc(td->count * sizeof(*data));
    while ((i = rpmtdNext(td)) >= 0) {
	data[i] = xstrdup(rpmtdGetString(td));
    }

    return newtd;
}
