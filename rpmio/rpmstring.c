/**
 * \file rpmio/rpmstring.c
 */

#include "system.h"

#include <stdarg.h>

#include <rpm/rpmstring.h>
#include "debug.h"

#define BUF_CHUNK 1024

struct StringBufRec {
    char *buf;
    char *tail;     /* Points to first "free" char */
    int allocated;
    int free;
};

char * stripTrailingChar(char * s, char c)
{
    char * t;
    for (t = s + strlen(s) - 1; *t == c && t >= s; t--)
	*t = '\0';
    return s;
}

char ** splitString(const char * str, size_t length, char sep)
{
    const char * source;
    char * s, * dest;
    char ** list;
    int i;
    int fields;

    s = xmalloc(length + 1);

    fields = 1;
    for (source = str, dest = s, i = 0; i < length; i++, source++, dest++) {
	*dest = *source;
	if (*dest == sep) fields++;
    }

    *dest = '\0';

    list = xmalloc(sizeof(*list) * (fields + 1));

    dest = s;
    list[0] = dest;
    i = 1;
    while (i < fields) {
	if (*dest == sep) {
	    list[i++] = dest + 1;
	    *dest = 0;
	}
	dest++;
    }

    list[i] = NULL;

/* FIX: list[i] is NULL */
    return list;
}

void freeSplitString(char ** list)
{
    list[0] = _free(list[0]);
    list = _free(list);
}

StringBuf newStringBuf(void)
{
    StringBuf sb = xmalloc(sizeof(*sb));

    sb->free = sb->allocated = BUF_CHUNK;
    sb->buf = xcalloc(sb->allocated, sizeof(*sb->buf));
    sb->buf[0] = '\0';
    sb->tail = sb->buf;
    
    return sb;
}

StringBuf freeStringBuf(StringBuf sb)
{
    if (sb) {
	sb->buf = _free(sb->buf);
	sb = _free(sb);
    }
    return sb;
}

void truncStringBuf(StringBuf sb)
{
    sb->buf[0] = '\0';
    sb->tail = sb->buf;
    sb->free = sb->allocated;
}

void stripTrailingBlanksStringBuf(StringBuf sb)
{
    while (sb->free != sb->allocated) {
	if (! risspace(*(sb->tail - 1)))
	    break;
	sb->free++;
	sb->tail--;
    }
    sb->tail[0] = '\0';
}

char * getStringBuf(StringBuf sb)
{
    return sb->buf;
}

void appendStringBufAux(StringBuf sb, const char *s, int nl)
{
    int l;

    l = strlen(s);
    /* If free == l there is no room for NULL terminator! */
    while ((l + nl + 1) > sb->free) {
        sb->allocated += BUF_CHUNK;
	sb->free += BUF_CHUNK;
        sb->buf = xrealloc(sb->buf, sb->allocated);
	sb->tail = sb->buf + (sb->allocated - sb->free);
    }
    
    /* FIX: shrug */
    strcpy(sb->tail, s);
    sb->tail += l;
    sb->free -= l;
    if (nl) {
        sb->tail[0] = '\n';
        sb->tail[1] = '\0';
	sb->tail++;
	sb->free--;
    }
}

int rstrcasecmp(const char * s1, const char * s2)
{
  const char * p1 = s1;
  const char * p2 = s2;
  char c1, c2;

  if (p1 == p2)
    return 0;

  do
    {
      c1 = rtolower (*p1++);
      c2 = rtolower (*p2++);
      if (c1 == '\0')
        break;
    }
  while (c1 == c2);

  return (int)(c1 - c2);
}

int rstrncasecmp(const char *s1, const char *s2, size_t n)
{
  const char * p1 = s1;
  const char * p2 = s2;
  char c1, c2;

  if (p1 == p2 || n == 0)
    return 0;

  do
    {
      c1 = rtolower (*p1++);
      c2 = rtolower (*p2++);
      if (c1 == '\0' || c1 != c2)
	break;
    } while (--n > 0);

  return (int)(c1 - c2);
}

/* 
 * Simple and stupid asprintf() clone.
 * FIXME: write to work with non-C99 vsnprintf or check for one in configure.
 */
int rasprintf(char **strp, const char *fmt, ...)
{
    int n;
    va_list ap;
    char * p = NULL;
  
    if (strp == NULL) 
	return -1;

    va_start(ap, fmt);
    n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (n >= -1) {
	size_t nb = n + 1;
	p = xmalloc(nb);
    	va_start(ap, fmt);
        n = vsnprintf(p, nb + 1, fmt, ap);
    	va_end(ap);
    } 
    *strp = p;
    return n;
}

