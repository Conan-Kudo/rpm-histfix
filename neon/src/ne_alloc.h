/* 
   Replacement memory allocation handling etc.
   Copyright (C) 1999-2004, Joe Orton <joe@manyfish.co.uk>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA

*/

#ifndef NE_ALLOC_H
#define NE_ALLOC_H

#ifdef WIN32
#include <stdlib.h>
#else
#include <sys/types.h>
#endif

#include "ne_defs.h"

BEGIN_NEON_DECLS

/* Set callback which is called if malloc() returns NULL. */
void ne_oom_callback(void (*callback)(void))
	/*@*/;

#ifndef NEON_MEMLEAK
/* Replacements for standard C library memory allocation functions,
 * which never return NULL. If the C library malloc() returns NULL,
 * neon will abort(); calling an OOM callback beforehand if one is
 * registered.  The C library will only ever return NULL if the
 * operating system does not use optimistic memory allocation. */
/*@mayexit@*/ /*@only@*/ /*@out@*/
void *ne_malloc(size_t size) ne_attribute_malloc
	/*@globals errno @*/
	/*@ensures maxSet(result) == (size - 1) @*/
	/*@modifies errno @*/;
/*@mayexit@*/ /*@only@*/
void *ne_calloc(size_t size) ne_attribute_malloc
	/*@*/;
/*@mayexit@*/ /*@only@*/
void *ne_realloc(void *ptr, size_t size) ne_attribute_malloc
        /*@ensures maxSet(result) == (size - 1) @*/
        /*@modifies *ptr @*/;
/*@mayexit@*/ /*@only@*/
char *ne_strdup(const char *s) ne_attribute_malloc
	/*@*/;
/*@mayexit@*/ /*@only@*/
char *ne_strndup(const char *s, size_t n) ne_attribute_malloc
	/*@*/;
#define ne_free free
#endif

/* Handy macro to free things: takes an lvalue, and sets to NULL
 * afterwards. */
#define NE_FREE(x) do { if ((x) != NULL) ne_free((x)); (x) = NULL; } while (0)

END_NEON_DECLS

#endif /* NE_ALLOC_H */
