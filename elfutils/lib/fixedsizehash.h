/* Fixed size hash table with internal linking.
   Copyright (C) 2000, 2001, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2000.

   This program is Open Source software; you can redistribute it and/or
   modify it under the terms of the Open Software License version 1.0 as
   published by the Open Source Initiative.

   You should have received a copy of the Open Software License along
   with this program; if not, you may obtain a copy of the Open Software
   License version 1.0 from http://www.opensource.org/licenses/osl.php or
   by writing the Open Source Initiative c/o Lawrence Rosen, Esq.,
   3001 King Ranch Road, Ukiah, CA 95482.   */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/cdefs.h>
#include <sys/param.h>

#include <system.h>

#define CONCAT(t1,t2) __CONCAT (t1,t2)

/* Before including this file the following macros must be defined:

   TYPE           data type of the hash table entries
   HASHFCT        name of the hashing function to use
   HASHTYPE       type used for the hashing value
   COMPARE        comparison function taking two pointers to TYPE objects
   CLASS          can be defined to `static' to avoid exporting the functions
   PREFIX         prefix to be used for function and data type names
   STORE_POINTER  if defined the table stores a pointer and not an element
                  of type TYPE
   INSERT_HASH    if defined alternate insert function which takes a hash
                  value is defined
   NO_FINI_FCT    if defined the fini function is not defined
*/


/* Defined separately.  */
extern size_t next_prime (size_t seed)
	/*@*/;


/* Set default values.  */
#ifndef HASHTYPE
# define HASHTYPE size_t
#endif

#ifndef CLASS
# define CLASS
#endif

#ifndef PREFIX
# define PREFIX
#endif


/* The data structure.  */
struct CONCAT(PREFIX,fshash)
{
  size_t nslots;
  struct CONCAT(PREFIX,fshashent)
  {
    HASHTYPE hval;
#ifdef STORE_POINTER
# define ENTRYP(el) (el).entry
    TYPE *entry;
#else
# define ENTRYP(el) &(el).entry
    TYPE entry;
#endif
  } table[0];
};


/* Constructor for the hashing table.  */
/*@null@*/
CLASS struct CONCAT(PREFIX,fshash) *
CONCAT(PREFIX,fshash_init) (size_t nelems)
{
  struct CONCAT(PREFIX,fshash) *result;
  /* We choose a size for the hashing table 150% over the number of
     entries.  This will guarantee short medium search lengths.  */
  const size_t max_size_t = ~((size_t) 0);

  if (nelems >= (max_size_t / 3) * 2)
    {
      errno = EINVAL;
      return NULL;
    }

  /* Adjust the size to be used for the hashing table.  */
  nelems = next_prime (MAX ((nelems * 3) / 2, 10));

  /* Allocate the data structure for the result.  */
  result = (struct CONCAT(PREFIX,fshash) *)
    xcalloc (sizeof (struct CONCAT(PREFIX,fshash))
	     + (nelems + 1) * sizeof (struct CONCAT(PREFIX,fshashent)), 1);
  if (result == NULL)
    return NULL;

  result->nslots = nelems;

  return result;
}


#ifndef NO_FINI_FCT
CLASS void
CONCAT(PREFIX,fshash_fini) (/*@only@*/ struct CONCAT(PREFIX,fshash) *htab)
	/*@modifies htab @*/
{
  free (htab);
}
#endif


static struct CONCAT(PREFIX,fshashent) *
CONCAT(PREFIX,fshash_lookup) (struct CONCAT(PREFIX,fshash) *htab,
			      HASHTYPE hval, TYPE *data)
	/*@*/
{
  size_t idx = 1 + hval % htab->nslots;

  if (htab->table[idx].hval != 0)
    {
      HASHTYPE hash;

      /* See whether this is the same entry.  */
      if (htab->table[idx].hval == hval
	  && COMPARE (data, ENTRYP (htab->table[idx])) == 0)
	return &htab->table[idx];

      /* Second hash function as suggested in [Knuth].  */
      hash = 1 + hval % (htab->nslots - 2);

      do
	{
	  if (idx <= hash)
	    idx = htab->nslots + idx - hash;
	  else
	    idx -= hash;

	  if (htab->table[idx].hval == hval
	      && COMPARE (data, ENTRYP(htab->table[idx])) == 0)
	    return &htab->table[idx];
	}
      while (htab->table[idx].hval != 0);
    }

  return &htab->table[idx];
}


CLASS int
__attribute__ ((unused))
CONCAT(PREFIX,fshash_insert) (struct CONCAT(PREFIX,fshash) *htab,
			      const char *str, size_t len, TYPE *data)
	/*@*/
{
  HASHTYPE hval = HASHFCT (str, len ?: strlen (str));
  struct CONCAT(PREFIX,fshashent) *slot;

  slot = CONCAT(PREFIX,fshash_lookup) (htab, hval, data);
 if (slot->hval != 0)
    /* We don't want to overwrite the old value.  */
    return -1;

  slot->hval = hval;
#ifdef STORE_POINTER
  slot->entry = data;
#else
  slot->entry = *data;
#endif

  return 0;
}


#ifdef INSERT_HASH
CLASS int
__attribute__ ((unused))
CONCAT(PREFIX,fshash_insert_hash) (struct CONCAT(PREFIX,fshash) *htab,
				   HASHTYPE hval, TYPE *data)
{
  struct CONCAT(PREFIX,fshashent) *slot;

  slot = CONCAT(PREFIX,fshash_lookup) (htab, hval, data);
  if (slot->hval != 0)
    /* We don't want to overwrite the old value.  */
    return -1;

  slot->hval = hval;
#ifdef STORE_POINTER
  slot->entry = data;
#else
  slot->entry = *data;
#endif

  return 0;
}
#endif


CLASS int
__attribute__ ((unused))
CONCAT(PREFIX,fshash_overwrite) (struct CONCAT(PREFIX,fshash) *htab,
				 const char *str, size_t len, TYPE *data)
	/*@*/
{
  HASHTYPE hval = HASHFCT (str, len ?: strlen (str));
  struct CONCAT(PREFIX,fshashent) *slot;

  slot = CONCAT(PREFIX,fshash_lookup) (htab, hval, data);
  slot->hval = hval;
#ifdef STORE_POINTER
  slot->entry = data;
#else
  slot->entry = *data;
#endif

  return 0;
}


/*@null@*/
const CLASS TYPE *
CONCAT(PREFIX,fshash_find) (const struct CONCAT(PREFIX,fshash) *htab,
			    const char *str, size_t len, TYPE *data)
{
  HASHTYPE hval = HASHFCT (str, len ?: strlen (str));
  struct CONCAT(PREFIX,fshashent) *slot;

  slot = CONCAT(PREFIX,fshash_lookup) ((struct CONCAT(PREFIX,fshash) *) htab,
				       hval, data);
  if (slot->hval == 0)
    /* Not found.  */
    return NULL;

  return ENTRYP(*slot);
}


/* Unset the macros we expect.  */
#undef TYPE
#undef HASHFCT
#undef HASHTYPE
#undef COMPARE
#undef CLASS
#undef PREFIX
#undef INSERT_HASH
#undef STORE_POINTER
#undef NO_FINI_FCT
