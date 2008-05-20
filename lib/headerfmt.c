/** \ingroup header
 * \file lib/headerfmt.c
 */

#include "system.h"

#include <rpm/header.h>
#include <rpm/rpmtag.h>
#include <rpm/rpmstring.h>
#include <rpm/rpmpgp.h>

#include "debug.h"

#define PARSER_BEGIN 	0
#define PARSER_IN_ARRAY 1
#define PARSER_IN_EXPR  2

/** \ingroup header
 * HEADER_EXT_FORMAT format function prototype.
 * This will only ever be passed RPM_INT32_TYPE or RPM_STRING_TYPE to
 * help keep things simple.
 *
 * @param td		tag data container
 * @param formatPrefix
 * @param padding
 * @return		formatted string
 */
typedef char * (*headerTagFormatFunction)
			(rpmtd td, char * formatPrefix, size_t padding);

extern void *rpmHeaderFormatFuncByName(const char *fmt);

/** \ingroup header
 */
typedef struct sprintfTag_s * sprintfTag;
struct sprintfTag_s {
    headerTagFormatFunction fmt;
    rpmTag tag;
    int justOne;
    int arrayCount;
    char * format;
    char * type;
    int pad;
};

/** \ingroup header
 */
typedef struct sprintfToken_s * sprintfToken;
struct sprintfToken_s {
    enum {
	PTOK_NONE = 0,
	PTOK_TAG,
	PTOK_ARRAY,
	PTOK_STRING,
	PTOK_COND
    } type;
    union {
	struct sprintfTag_s tag;	/*!< PTOK_TAG */
	struct {
	    sprintfToken format;
	    int i;
	    int numTokens;
	} array;			/*!< PTOK_ARRAY */
	struct {
	    char * string;
	    int len;
	} string;			/*!< PTOK_STRING */
	struct {
	    sprintfToken ifFormat;
	    int numIfTokens;
	    sprintfToken elseFormat;
	    int numElseTokens;
	    struct sprintfTag_s tag;
	} cond;				/*!< PTOK_COND */
    } u;
};

/**
 */
typedef struct headerSprintfArgs_s {
    Header h;
    char * fmt;
    const char * errmsg;
    rpmtd *cache;
    sprintfToken format;
    HeaderIterator hi;
    char * val;
    size_t vallen;
    size_t alloced;
    int numTokens;
    int i;
} * headerSprintfArgs;


static char escapedChar(const char ch)	
{
    switch (ch) {
    case 'a': 	return '\a';
    case 'b': 	return '\b';
    case 'f': 	return '\f';
    case 'n': 	return '\n';
    case 'r': 	return '\r';
    case 't': 	return '\t';
    case 'v': 	return '\v';
    default:	return ch;
    }
}

/**
 * Destroy headerSprintf format array.
 * @param format	sprintf format array
 * @param num		number of elements
 * @return		NULL always
 */
static sprintfToken
freeFormat( sprintfToken format, int num)
{
    int i;

    if (format == NULL) return NULL;

    for (i = 0; i < num; i++) {
	switch (format[i].type) {
	case PTOK_ARRAY:
	    format[i].u.array.format =
		freeFormat(format[i].u.array.format,
			format[i].u.array.numTokens);
	    break;
	case PTOK_COND:
	    format[i].u.cond.ifFormat =
		freeFormat(format[i].u.cond.ifFormat, 
			format[i].u.cond.numIfTokens);
	    format[i].u.cond.elseFormat =
		freeFormat(format[i].u.cond.elseFormat, 
			format[i].u.cond.numElseTokens);
	    break;
	case PTOK_NONE:
	case PTOK_TAG:
	case PTOK_STRING:
	default:
	    break;
	}
    }
    format = _free(format);
    return NULL;
}

/**
 * Initialize an hsa iteration.
 * @param hsa		headerSprintf args
 */
static void hsaInit(headerSprintfArgs hsa)
{
    sprintfTag tag =
	(hsa->format->type == PTOK_TAG
	    ? &hsa->format->u.tag :
	(hsa->format->type == PTOK_ARRAY
	    ? &hsa->format->u.array.format->u.tag :
	NULL));

    hsa->i = 0;
    if (tag != NULL && tag->tag == -2)
	hsa->hi = headerInitIterator(hsa->h);
}

/**
 * Return next hsa iteration item.
 * @param hsa		headerSprintf args
 * @return		next sprintfToken (or NULL)
 */
static sprintfToken hsaNext(headerSprintfArgs hsa)
{
    sprintfToken fmt = NULL;
    sprintfTag tag =
	(hsa->format->type == PTOK_TAG
	    ? &hsa->format->u.tag :
	(hsa->format->type == PTOK_ARRAY
	    ? &hsa->format->u.array.format->u.tag :
	NULL));

    if (hsa->i >= 0 && hsa->i < hsa->numTokens) {
	fmt = hsa->format + hsa->i;
	if (hsa->hi == NULL) {
	    hsa->i++;
	} else {
	    rpmTag tagno;
	    rpmTagType type;
	    rpm_count_t count;

	    if (!headerNextIterator(hsa->hi, &tagno, &type, NULL, &count))
		fmt = NULL;
	    tag->tag = tagno;
	}
    }

    return fmt;
}

/**
 * Finish an hsa iteration.
 * @param hsa		headerSprintf args
 */
static void hsaFini(headerSprintfArgs hsa)
{
    hsa->hi = headerFreeIterator(hsa->hi);
    hsa->i = 0;
}

/**
 * Reserve sufficient buffer space for next output value.
 * @param hsa		headerSprintf args
 * @param need		no. of bytes to reserve
 * @return		pointer to reserved space
 */
static char * hsaReserve(headerSprintfArgs hsa, size_t need)
{
    if ((hsa->vallen + need) >= hsa->alloced) {
	if (hsa->alloced <= need)
	    hsa->alloced += need;
	hsa->alloced <<= 1;
	hsa->val = xrealloc(hsa->val, hsa->alloced+1);	
    }
    return hsa->val + hsa->vallen;
}

/**
 * Search tags for a name.
 * @param hsa		headerSprintf args
 * @param token		parsed fields
 * @param name		name to find
 * @return		0 on success, 1 on not found
 */
static int findTag(headerSprintfArgs hsa, sprintfToken token, const char * name)
{
    const char *tagname = name;
    sprintfTag stag = (token->type == PTOK_COND
	? &token->u.cond.tag : &token->u.tag);

    stag->fmt = NULL;
    stag->tag = -1;

    if (!strcmp(tagname, "*")) {
	stag->tag = -2;
	goto bingo;
    }

    if (strncmp("RPMTAG_", tagname, sizeof("RPMTAG_")-1) == 0) {
	tagname += sizeof("RPMTAG");
    }

    /* Search tag names. */
    stag->tag = rpmTagGetValue(tagname);
    if (stag->tag != RPMTAG_NOT_FOUND)
	goto bingo;

    return 1;

bingo:
    /* Search extensions for specific format. */
    if (stag->type != NULL)
	stag->fmt = rpmHeaderFormatFuncByName(stag->type);

    return 0;
}

/* forward ref */
/**
 * Parse an expression.
 * @param hsa		headerSprintf args
 * @param token		token
 * @param str		string
 * @param[out] *endPtr
 * @return		0 on success
 */
static int parseExpression(headerSprintfArgs hsa, sprintfToken token,
		char * str,char ** endPtr);

/**
 * Parse a headerSprintf term.
 * @param hsa		headerSprintf args
 * @param str
 * @retval *formatPtr
 * @retval *numTokensPtr
 * @retval *endPtr
 * @param state
 * @return		0 on success
 */
static int parseFormat(headerSprintfArgs hsa, char * str,
	sprintfToken * formatPtr,int * numTokensPtr,
	char ** endPtr, int state)
{
    char * chptr, * start, * next, * dst;
    sprintfToken format;
    sprintfToken token;
    int numTokens;
    int i;
    int done = 0;

    /* upper limit on number of individual formats */
    numTokens = 0;
    if (str != NULL)
    for (chptr = str; *chptr != '\0'; chptr++)
	if (*chptr == '%') numTokens++;
    numTokens = numTokens * 2 + 1;

    format = xcalloc(numTokens, sizeof(*format));
    if (endPtr) *endPtr = NULL;

    /* LCL: can't detect done termination */
    dst = start = str;
    numTokens = 0;
    token = NULL;
    if (start != NULL)
    while (*start != '\0') {
	switch (*start) {
	case '%':
	    /* handle %% */
	    if (*(start + 1) == '%') {
		if (token == NULL || token->type != PTOK_STRING) {
		    token = format + numTokens++;
		    token->type = PTOK_STRING;
		    dst = token->u.string.string = start;
		}
		start++;
		*dst++ = *start++;
		break;
	    } 

	    token = format + numTokens++;
	    *dst++ = '\0';
	    start++;

	    if (*start == '|') {
		char * newEnd;

		start++;
		if (parseExpression(hsa, token, start, &newEnd))
		{
		    format = freeFormat(format, numTokens);
		    return 1;
		}
		start = newEnd;
		break;
	    }

	    token->u.tag.format = start;
	    token->u.tag.pad = 0;
	    token->u.tag.justOne = 0;
	    token->u.tag.arrayCount = 0;

	    chptr = start;
	    while (*chptr && *chptr != '{' && *chptr != '%') chptr++;
	    if (!*chptr || *chptr == '%') {
		hsa->errmsg = _("missing { after %");
		format = freeFormat(format, numTokens);
		return 1;
	    }

	    *chptr++ = '\0';

	    while (start < chptr) {
		if (risdigit(*start)) {
		    i = strtoul(start, &start, 10);
		    token->u.tag.pad += i;
		    start = chptr;
		    break;
		} else {
		    start++;
		}
	    }

	    if (*start == '=') {
		token->u.tag.justOne = 1;
		start++;
	    } else if (*start == '#') {
		token->u.tag.justOne = 1;
		token->u.tag.arrayCount = 1;
		start++;
	    }

	    dst = next = start;
	    while (*next && *next != '}') next++;
	    if (!*next) {
		hsa->errmsg = _("missing } after %{");
		format = freeFormat(format, numTokens);
		return 1;
	    }
	    *next++ = '\0';

	    chptr = start;
	    while (*chptr && *chptr != ':') chptr++;

	    if (*chptr != '\0') {
		*chptr++ = '\0';
		if (!*chptr) {
		    hsa->errmsg = _("empty tag format");
		    format = freeFormat(format, numTokens);
		    return 1;
		}
		token->u.tag.type = chptr;
	    } else {
		token->u.tag.type = "string";
	    }
	    
	    if (!*start) {
		hsa->errmsg = _("empty tag name");
		format = freeFormat(format, numTokens);
		return 1;
	    }

	    i = 0;
	    token->type = PTOK_TAG;

	    if (findTag(hsa, token, start)) {
		hsa->errmsg = _("unknown tag");
		format = freeFormat(format, numTokens);
		return 1;
	    }

	    start = next;
	    break;

	case '[':
	    *dst++ = '\0';
	    *start++ = '\0';
	    token = format + numTokens++;

	    if (parseFormat(hsa, start,
			    &token->u.array.format,
			    &token->u.array.numTokens,
			    &start, PARSER_IN_ARRAY))
	    {
		format = freeFormat(format, numTokens);
		return 1;
	    }

	    if (!start) {
		hsa->errmsg = _("] expected at end of array");
		format = freeFormat(format, numTokens);
		return 1;
	    }

	    dst = start;

	    token->type = PTOK_ARRAY;

	    break;

	case ']':
	    if (state != PARSER_IN_ARRAY) {
		hsa->errmsg = _("unexpected ]");
		format = freeFormat(format, numTokens);
		return 1;
	    }
	    *start++ = '\0';
	    if (endPtr) *endPtr = start;
	    done = 1;
	    break;

	case '}':
	    if (state != PARSER_IN_EXPR) {
		hsa->errmsg = _("unexpected }");
		format = freeFormat(format, numTokens);
		return 1;
	    }
	    *start++ = '\0';
	    if (endPtr) *endPtr = start;
	    done = 1;
	    break;

	default:
	    if (token == NULL || token->type != PTOK_STRING) {
		token = format + numTokens++;
		token->type = PTOK_STRING;
		dst = token->u.string.string = start;
	    }

	    if (*start == '\\') {
		start++;
		*dst++ = escapedChar(*start++);
	    } else {
		*dst++ = *start++;
	    }
	    break;
	}
	if (done)
	    break;
    }

    if (dst != NULL)
        *dst = '\0';

    for (i = 0; i < numTokens; i++) {
	token = format + i;
	if (token->type == PTOK_STRING)
	    token->u.string.len = strlen(token->u.string.string);
    }

    *numTokensPtr = numTokens;
    *formatPtr = format;

    return 0;
}

static int parseExpression(headerSprintfArgs hsa, sprintfToken token,
		char * str, char ** endPtr)
{
    char * chptr;
    char * end;

    hsa->errmsg = NULL;
    chptr = str;
    while (*chptr && *chptr != '?') chptr++;

    if (*chptr != '?') {
	hsa->errmsg = _("? expected in expression");
	return 1;
    }

    *chptr++ = '\0';;

    if (*chptr != '{') {
	hsa->errmsg = _("{ expected after ? in expression");
	return 1;
    }

    chptr++;

    if (parseFormat(hsa, chptr, &token->u.cond.ifFormat, 
		    &token->u.cond.numIfTokens, &end, PARSER_IN_EXPR)) 
	return 1;

    /* XXX fix segfault on "rpm -q rpm --qf='%|NAME?{%}:{NAME}|\n'"*/
    if (!(end && *end)) {
	hsa->errmsg = _("} expected in expression");
	token->u.cond.ifFormat =
		freeFormat(token->u.cond.ifFormat, token->u.cond.numIfTokens);
	return 1;
    }

    chptr = end;
    if (*chptr != ':' && *chptr != '|') {
	hsa->errmsg = _(": expected following ? subexpression");
	token->u.cond.ifFormat =
		freeFormat(token->u.cond.ifFormat, token->u.cond.numIfTokens);
	return 1;
    }

    if (*chptr == '|') {
	if (parseFormat(hsa, NULL, &token->u.cond.elseFormat, 
		&token->u.cond.numElseTokens, &end, PARSER_IN_EXPR))
	{
	    token->u.cond.ifFormat =
		freeFormat(token->u.cond.ifFormat, token->u.cond.numIfTokens);
	    return 1;
	}
    } else {
	chptr++;

	if (*chptr != '{') {
	    hsa->errmsg = _("{ expected after : in expression");
	    token->u.cond.ifFormat =
		freeFormat(token->u.cond.ifFormat, token->u.cond.numIfTokens);
	    return 1;
	}

	chptr++;

	if (parseFormat(hsa, chptr, &token->u.cond.elseFormat, 
			&token->u.cond.numElseTokens, &end, PARSER_IN_EXPR)) 
	    return 1;

	/* XXX fix segfault on "rpm -q rpm --qf='%|NAME?{a}:{%}|{NAME}\n'" */
	if (!(end && *end)) {
	    hsa->errmsg = _("} expected in expression");
	    token->u.cond.ifFormat =
		freeFormat(token->u.cond.ifFormat, token->u.cond.numIfTokens);
	    return 1;
	}

	chptr = end;
	if (*chptr != '|') {
	    hsa->errmsg = _("| expected at end of expression");
	    token->u.cond.ifFormat =
		freeFormat(token->u.cond.ifFormat, token->u.cond.numIfTokens);
	    token->u.cond.elseFormat =
		freeFormat(token->u.cond.elseFormat, token->u.cond.numElseTokens);
	    return 1;
	}
    }
	
    chptr++;

    *endPtr = chptr;

    token->type = PTOK_COND;

    (void) findTag(hsa, token, str);

    return 0;
}

static rpmtd getCached(rpmtd *cache, rpmTag tag)
{
    rpmtd td = NULL;

    if (tag >= HEADER_IMAGE && tag < RPMTAG_FIRSTFREE_TAG && cache[tag]) {
	td = cache[tag];
    }
    return td;
}

/**
 * Do headerGet() just once for given tag, cache results.
 * @param hsa		headerSprintf args
 * @param tag
 * @retval *typeptr
 * @retval *data
 * @retval *countptr
 * @return		1 on success, 0 on failure
 */
static rpmtd getData(headerSprintfArgs hsa, rpmTag tag)
{
    rpmtd td = NULL;

    if (!(td = getCached(hsa->cache, tag))) {
	td = rpmtdNew();
	if (!headerGet(hsa->h, tag, td, HEADERGET_EXT)) {
	    rpmtdFree(td);
	    return NULL;
	}
	hsa->cache[tag] = td;
    }

    return td;
}

/**
 * formatValue
 * @param hsa		headerSprintf args
 * @param tag
 * @param element
 * @return		end of formatted string (NULL on error)
 */
static char * formatValue(headerSprintfArgs hsa, sprintfTag tag, int element)
{
    char * val = NULL;
    size_t need = 0;
    char * t, * te;
    char buf[20];
    unsigned int intVal;
    const char *str;
    int countBuf;
    struct rpmtd_s tmp;
    rpmtd td;

    memset(buf, 0, sizeof(buf));
    if (!(td = getData(hsa, tag->tag))) {
	tmp.count = 1;
	tmp.type = RPM_STRING_TYPE;
	tmp.data = "(none)";
	element = 0;
	td = &tmp;	
    }

    if (tag->arrayCount) {
	countBuf = td->count;
	tmp.data = &countBuf;
	tmp.count = 1;
	tmp.type = RPM_INT32_TYPE;
	element = 0;
	td = &tmp;
    }
    td->ix = element; /* Ick, use iterators instead */

    (void) stpcpy( stpcpy(buf, "%"), tag->format);

    val = tag->fmt(td, buf, tag->pad);
    need = strlen(val);

    if (val && need > 0) {
	t = hsaReserve(hsa, need);
	te = stpcpy(t, val);
	hsa->vallen += (te - t);
	val = _free(val);
    }

    return (hsa->val + hsa->vallen);
}

/**
 * Format a single headerSprintf item.
 * @param hsa		headerSprintf args
 * @param token
 * @param element
 * @return		end of formatted string (NULL on error)
 */
static char * singleSprintf(headerSprintfArgs hsa, sprintfToken token,
		int element)
{
    char * t, * te;
    int i, j, found;
    rpm_count_t count, numElements;
    sprintfToken spft;
    int condNumFormats;
    size_t need;

    /* we assume the token and header have been validated already! */

    switch (token->type) {
    case PTOK_NONE:
	break;

    case PTOK_STRING:
	need = token->u.string.len;
	if (need == 0) break;
	t = hsaReserve(hsa, need);
	te = stpcpy(t, token->u.string.string);
	hsa->vallen += (te - t);
	break;

    case PTOK_TAG:
	t = hsa->val + hsa->vallen;
	te = formatValue(hsa, &token->u.tag,
			(token->u.tag.justOne ? 0 : element));
	if (te == NULL)
	    return NULL;
	break;

    case PTOK_COND:
	if (getCached(hsa->cache, token->u.cond.tag.tag) ||
		      headerIsEntry(hsa->h, token->u.cond.tag.tag)) {
	    spft = token->u.cond.ifFormat;
	    condNumFormats = token->u.cond.numIfTokens;
	} else {
	    spft = token->u.cond.elseFormat;
	    condNumFormats = token->u.cond.numElseTokens;
	}

	need = condNumFormats * 20;
	if (spft == NULL || need == 0) break;

	t = hsaReserve(hsa, need);
	for (i = 0; i < condNumFormats; i++, spft++) {
	    te = singleSprintf(hsa, spft, element);
	    if (te == NULL)
		return NULL;
	}
	break;

    case PTOK_ARRAY:
	numElements = 0;
	found = 0;
	spft = token->u.array.format;
	for (i = 0; i < token->u.array.numTokens; i++, spft++)
	{
	    rpmtd td = NULL;
	    if (spft->type != PTOK_TAG ||
		spft->u.tag.arrayCount ||
		spft->u.tag.justOne) continue;

	    if (!(td = getData(hsa, spft->u.tag.tag))) {
		continue;
	    }

	    found = 1;

	    if (td->type == RPM_BIN_TYPE) {
		count = 1;	/* XXX count abused as no. of bytes. */
	    } else {
		count = td->count;
	    }

	    if (numElements > 1 && count != numElements)
	    switch (td->type) {
	    default:
		hsa->errmsg =
			_("array iterator used with different sized arrays");
		return NULL;
		break;
	    case RPM_BIN_TYPE:
	    case RPM_STRING_TYPE:
		break;
	    }
	    if (count > numElements)
		numElements = count;
	}

	if (! found) {
	    need = sizeof("(none)") - 1;
	    t = hsaReserve(hsa, need);
	    te = stpcpy(t, "(none)");
	    hsa->vallen += (te - t);
	} else {
	    int isxml;

	    need = numElements * token->u.array.numTokens * 10;
	    if (need == 0) break;

	    spft = token->u.array.format;
	    isxml = (spft->type == PTOK_TAG && spft->u.tag.type != NULL &&
		!strcmp(spft->u.tag.type, "xml"));

	    if (isxml) {
		const char * tagN = rpmTagGetName(spft->u.tag.tag);

		need = sizeof("  <rpmTag name=\"\">\n") - 1;
		if (tagN != NULL)
		    need += strlen(tagN);
		t = hsaReserve(hsa, need);
		te = stpcpy(t, "  <rpmTag name=\"");
		if (tagN != NULL)
		    te = stpcpy(te, tagN);
		te = stpcpy(te, "\">\n");
		hsa->vallen += (te - t);
	    }

	    t = hsaReserve(hsa, need);
	    for (j = 0; j < numElements; j++) {
		spft = token->u.array.format;
		for (i = 0; i < token->u.array.numTokens; i++, spft++) {
		    te = singleSprintf(hsa, spft, j);
		    if (te == NULL)
			return NULL;
		}
	    }

	    if (isxml) {
		need = sizeof("  </rpmTag>\n") - 1;
		t = hsaReserve(hsa, need);
		te = stpcpy(t, "  </rpmTag>\n");
		hsa->vallen += (te - t);
	    }

	}
	break;
    }

    return (hsa->val + hsa->vallen);
}

/**
 * Create tag data cache.
 * This allocates much more space than necessary but playing it
 * simple and stupid for now.
 */
static rpmtd *cacheCreate(void)
{
    rpmtd *cache = xcalloc(RPMTAG_FIRSTFREE_TAG, sizeof(*cache));
    return cache;
}

/**
 * Free tag data cache contents and destroy cache.
 */
static void *cacheFree(rpmtd *cache)
{
    rpmtd *td = cache;
    for (int i = 0; i < RPMTAG_FIRSTFREE_TAG; i++, td++) {
	if (*td) {
	    rpmtdFreeData(*td);
	    rpmtdFree(*td);
	}
    }
    free(cache);
    return NULL;
}

char * headerFormat(Header h, const char * fmt, errmsg_t * errmsg) 
{
    struct headerSprintfArgs_s hsa;
    sprintfToken nextfmt;
    sprintfTag tag;
    char * t, * te;
    int isxml;
    size_t need;
 
    memset(&hsa, 0, sizeof(hsa));
    hsa.h = headerLink(h);
    hsa.fmt = xstrdup(fmt);
    hsa.errmsg = NULL;

    if (parseFormat(&hsa, hsa.fmt, &hsa.format, &hsa.numTokens, NULL, PARSER_BEGIN))
	goto exit;

    hsa.cache = cacheCreate();
    hsa.val = xstrdup("");

    tag =
	(hsa.format->type == PTOK_TAG
	    ? &hsa.format->u.tag :
	(hsa.format->type == PTOK_ARRAY
	    ? &hsa.format->u.array.format->u.tag :
	NULL));
    isxml = (tag != NULL && tag->tag == -2 && tag->type != NULL && !strcmp(tag->type, "xml"));

    if (isxml) {
	need = sizeof("<rpmHeader>\n") - 1;
	t = hsaReserve(&hsa, need);
	te = stpcpy(t, "<rpmHeader>\n");
	hsa.vallen += (te - t);
    }

    hsaInit(&hsa);
    while ((nextfmt = hsaNext(&hsa)) != NULL) {
	te = singleSprintf(&hsa, nextfmt, 0);
	if (te == NULL) {
	    hsa.val = _free(hsa.val);
	    break;
	}
    }
    hsaFini(&hsa);

    if (isxml) {
	need = sizeof("</rpmHeader>\n") - 1;
	t = hsaReserve(&hsa, need);
	te = stpcpy(t, "</rpmHeader>\n");
	hsa.vallen += (te - t);
    }

    if (hsa.val != NULL && hsa.vallen < hsa.alloced)
	hsa.val = xrealloc(hsa.val, hsa.vallen+1);	

    hsa.cache = cacheFree(hsa.cache);
    hsa.format = freeFormat(hsa.format, hsa.numTokens);

exit:
    if (errmsg)
	*errmsg = hsa.errmsg;
    hsa.h = headerFree(hsa.h);
    hsa.fmt = _free(hsa.fmt);
    return hsa.val;
}

char * headerSprintf(Header h, const char * fmt,
		     void * tbltags,
		     void * extensions,
		     errmsg_t * errmsg)
{
    return headerFormat(h, fmt, errmsg);
}

