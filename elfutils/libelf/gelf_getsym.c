/* Get symbol information from symbol table at the given index.
   Copyright (C) 1999, 2000, 2001, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 1999.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2 as
   published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <gelf.h>
#include <string.h>

#include "libelfP.h"


GElf_Sym *
gelf_getsym (Elf_Data *data, int ndx, GElf_Sym *dst)
{
  Elf_Data_Scn *data_scn = (Elf_Data_Scn *) data;
  GElf_Sym *result = NULL;

  if (data == NULL)
    return NULL;

  if (unlikely (data->d_type != ELF_T_SYM))
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return NULL;
    }

  rwlock_rdlock (data_scn->s->elf->lock);

  /* This is the one place where we have to take advantage of the fact
     that an `Elf_Data' pointer is also a pointer to `Elf_Data_Scn'.
     The interface is broken so that it requires this hack.  */
  if (data_scn->s->elf->class == ELFCLASS32)
    {
      Elf32_Sym *src;

      /* Here it gets a bit more complicated.  The format of the symbol
	 table entries has to be adopted.  The user better has provided
	 a buffer where we can store the information.  While copying the
	 data we are converting the format.  */
      if (unlikely ((ndx + 1) * sizeof (Elf32_Sym) > data->d_size))
	{
	  __libelf_seterrno (ELF_E_INVALID_INDEX);
	  goto out;
	}

      src = &((Elf32_Sym *) data->d_buf)[ndx];

      /* This might look like a simple copy operation but it's
	 not.  There are zero- and sign-extensions going on.  */
#define COPY(name) \
      dst->name = src->name
      COPY (st_name);
      /* Please note that we can simply copy the `st_info' element since
	 the definitions of ELFxx_ST_BIND and ELFxx_ST_TYPE are the same
	 for the 64 bit variant.  */
      COPY (st_info);
      COPY (st_other);
      COPY (st_shndx);
      COPY (st_value);
      COPY (st_size);
    }
  else
    {
      /* If this is a 64 bit object it's easy.  */
      assert (sizeof (GElf_Sym) == sizeof (Elf64_Sym));

      /* The data is already in the correct form.  Just make sure the
	 index is OK.  */
      if (unlikely ((ndx + 1) * sizeof (GElf_Sym) > data->d_size))
	{
	  __libelf_seterrno (ELF_E_INVALID_INDEX);
	  goto out;
	}

      *dst = ((GElf_Sym *) data->d_buf)[ndx];
    }

  result = dst;

 out:
  rwlock_unlock (data_scn->s->elf->lock);

  return result;
}
INTDEF(gelf_getsym)
