/* Get information from dynamic table at the given index.
   Copyright (C) 2000, 2001, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2000.

   This program is Open Source software; you can redistribute it and/or
   modify it under the terms of the Open Software License version 1.0 as
   published by the Open Source Initiative.

   You should have received a copy of the Open Software License along
   with this program; if not, you may obtain a copy of the Open Software
   License version 1.0 from http://www.opensource.org/license/osl.php or
   by writing the Open Source Initiative c/o Lawrence Rosen, Esq.,
   3001 King Ranch Road, Ukiah, CA 95482.   */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <gelf.h>
#include <string.h>

#include "libelfP.h"


GElf_Dyn *
gelf_getdyn (Elf_Data *data, int ndx, GElf_Dyn *dst)
{
  Elf_Data_Scn *data_scn = (Elf_Data_Scn *) data;
  GElf_Dyn *result = NULL;
  Elf *elf;

  if (data_scn == NULL)
    return NULL;

  if (unlikely (data_scn->d.d_type != ELF_T_DYN))
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return NULL;
    }

  elf = data_scn->s->elf;

  rwlock_rdlock (elf->lock);

  /* This is the one place where we have to take advantage of the fact
     that an `Elf_Data' pointer is also a pointer to `Elf_Data_Scn'.
     The interface is broken so that it requires this hack.  */
  if (elf->class == ELFCLASS32)
    {
      Elf32_Dyn *src;

      /* Here it gets a bit more complicated.  The format of the symbol
	 table entries has to be adopted.  The user better has provided
	 a buffer where we can store the information.  While copying the
	 data we are converting the format.  */
      if (unlikely ((ndx + 1) * sizeof (Elf32_Dyn) > data_scn->d.d_size))
	{
	  __libelf_seterrno (ELF_E_INVALID_INDEX);
	  goto out;
	}

      src = &((Elf32_Dyn *) data_scn->d.d_buf)[ndx];

      /* This might look like a simple copy operation but it's
	 not.  There are zero- and sign-extensions going on.  */
      dst->d_tag = src->d_tag;
      /* It OK to copy `d_val' since `d_ptr' has the same size.  */
      dst->d_un.d_val = src->d_un.d_val;
    }
  else
    {
      /* If this is a 64 bit object it's easy.  */
      assert (sizeof (GElf_Dyn) == sizeof (Elf64_Dyn));

      /* The data is already in the correct form.  Just make sure the
	 index is OK.  */
      if (unlikely ((ndx + 1) * sizeof (GElf_Dyn) > data_scn->d.d_size))
	{
	  __libelf_seterrno (ELF_E_INVALID_INDEX);
	  goto out;
	}

      *dst = ((GElf_Dyn *) data_scn->d.d_buf)[ndx];
    }

  result = dst;

 out:
  rwlock_unlock (elf->lock);

  return result;
}
