#include "system.h"

#include <err.h>	/* XXX !HAVE_ERR_H: get from misc */

void *vmefail(void)
{
    err(EXIT_FAILURE, "virtual memory exhausted");
    return NULL;
}

#if !(HAVE_MCHECK_H && defined(__GNUC__))

void * xmalloc (size_t size)
{
    register void *value = malloc (size);
    if (value == 0)
	value = vmefail();
    return value;
}

void * xcalloc (size_t nmemb, size_t size)
{
    register void *value = calloc (nmemb, size);
    if (value == 0)
	value = vmefail();
    return value;
}

void * xrealloc (void *ptr, size_t size)
{
    register void *value = realloc (ptr, size);
    if (value == 0)
	value = vmefail();
    return value;
}

char * xstrdup (const char *str)
{
    char *newstr = (char *) malloc (strlen(str) + 1);
    if (newstr == 0)
	newstr = (char *) vmefail();
    strcpy (newstr, str);
    return newstr;
}

#endif	/* !(HAVE_MCHECK_H && defined(__GNUC__)) */
