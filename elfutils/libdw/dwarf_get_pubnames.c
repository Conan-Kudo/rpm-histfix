/* Get public symbol information.
   Copyright (C) 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2002.

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
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>

#include <libdwP.h>


static int
get_offsets (Dwarf *dbg)
{
  size_t allocated = 0;
  size_t cnt = 0;
  struct pubnames_s *mem = NULL;
  const size_t entsize = sizeof (struct pubnames_s);
  unsigned char *const startp = dbg->sectiondata[IDX_debug_pubnames]->d_buf;
  unsigned char *readp = startp;
  unsigned char *endp = readp + dbg->sectiondata[IDX_debug_pubnames]->d_size;

  while (readp + 14 < endp)
    {
      int len_bytes;
      Dwarf_Off len;
      uint16_t version;
      unsigned char *infop;

      /* If necessary, allocate more entries.  */
      if (cnt >= allocated)
	{
	  struct pubnames_s *newmem;
	  allocated = MAX (10, 2 * allocated);
	  newmem = (struct pubnames_s *) realloc (mem, allocated * entsize);
	  if (newmem == NULL)
	    {
	      __libdwarf_seterrno (DWARF_E_NOMEM);
	    err_return:
	      free (mem);
	      return -1;
	    }

	  mem = newmem;
	}

      /* Read the set header.  */
      len_bytes = 4;
      len = read_4ubyte_unaligned_inc (dbg, readp);
      if (len == 0xffffffff)
	{
	  len = read_8ubyte_unaligned_inc (dbg, readp);
	  len_bytes = 8;
	}

      /* Now we know the offset of the first offset/name pair.  */
      mem[cnt].set_start = readp + 2 + 2 * len_bytes - startp;
      mem[cnt].address_len = len_bytes;
      if (mem[cnt].set_start >= dbg->sectiondata[IDX_debug_pubnames]->d_size)
	/* Something wrong, the first entry is beyond the end of
	   the section.  */
	break;

      /* Read the version.  It better be two for now.  */
      version = read_2ubyte_unaligned (dbg, readp);
      if (version != 2)
	{
	  __libdwarf_seterrno (DWARF_E_INVALID_VERSION);
	  goto err_return;
	}

      /* Get the CU offset.  */
      if (len_bytes == 4)
	mem[cnt].cu_offset = read_4ubyte_unaligned (dbg, readp + 2);
      else
	mem[cnt].cu_offset = read_8ubyte_unaligned (dbg, readp + 2);

      /* Determine the size of the CU header.  */
      assert (dbg->sectiondata[IDX_debug_info] != NULL);
      assert (dbg->sectiondata[IDX_debug_info]->d_buf != NULL);
      assert (mem[cnt].cu_offset + 3
	      < dbg->sectiondata[IDX_debug_info]->d_size);
      infop = ((unsigned char *) dbg->sectiondata[IDX_debug_info]->d_buf
	   + mem[cnt].cu_offset);
      if (read_4ubyte_unaligned_noncvt (infop) == 0xffffffff)
	mem[cnt].cu_header_size = 23;
      else
	mem[cnt].cu_header_size = 11;

      ++cnt;

      /* Advance to the next set.  */
      readp += len;
    }

  if (mem == NULL)
    {
      __libdwarf_seterrno (DWARF_E_NO_ENTRY);
      return -1;
    }

  dbg->pubnames_sets = (struct pubnames_s *) realloc (mem, cnt * entsize);
  dbg->pubnames_nsets = cnt;

  return 0;
}


size_t
dwarf_get_pubnames (dbg, callback, arg, offset)
     Dwarf *dbg;
     int (*callback) (Dwarf *, Dwarf_Global *, void *);
     void *arg;
     size_t offset;
{
  size_t cnt;
  unsigned char *startp;
  unsigned char *readp;

  /* Make sure it is a valid offset.  */
  if (unlikely (dbg->sectiondata[IDX_debug_pubnames] == NULL
		|| offset >= dbg->sectiondata[IDX_debug_pubnames]->d_size))
    /* No (more) entry.  */
    return 0;

  /* If necessary read the set information.  */
  if (dbg->pubnames_nsets == 0 && get_offsets (dbg) != 0)
    return (size_t) -1;

  /* Find the place where to start.  */
  if (offset == 0)
    {
      cnt = 0;
      offset = dbg->pubnames_sets[0].set_start;
    }
  else
    {
      for (cnt = 0; cnt + 1 < dbg->pubnames_nsets; ++cnt)
	if (offset >= dbg->pubnames_sets[cnt].set_start)
	  {
	    assert (offset < dbg->pubnames_sets[cnt + 1].set_start);
	    break;
	  }
      assert (cnt + 1 < dbg->pubnames_nsets);
    }

  startp = (unsigned char *) dbg->sectiondata[IDX_debug_pubnames]->d_buf;
  readp = startp + offset;
  while (1)
    {
      Dwarf_Global gl;

      gl.cu_offset = (dbg->pubnames_sets[cnt].cu_offset
		      + dbg->pubnames_sets[cnt].cu_header_size);

      while (1)
	{
	  /* READP points to the next offset/name pair.  */
	  if (dbg->pubnames_sets[cnt].address_len == 4)
	    gl.die_offset = read_4ubyte_unaligned_inc (dbg, readp);
	  else
	    gl.die_offset = read_8ubyte_unaligned_inc (dbg, readp);

	  /* If the offset is zero we reached the end of the set.  */
	  if (gl.die_offset == 0)
	    break;

	  /* Add the CU offset.  */
	  gl.die_offset += dbg->pubnames_sets[cnt].cu_offset;

	  gl.name = (char *) readp;
	  readp = (unsigned char *) rawmemchr (gl.name, '\0') + 1;

	  /* We found name and DIE offset.  Report it.  */
	  if (callback (dbg, &gl, arg) != DWARF_CB_OK)
	    {
	      /* The user wants us to stop.  Return the offset of the
		 next entry.  */
	      return readp - startp;
	    }
	}

      if (++cnt == dbg->pubnames_nsets)
	/* This was the last set.  */
	break;

      startp = (unsigned char *) dbg->sectiondata[IDX_debug_pubnames]->d_buf;
      readp = startp + dbg->pubnames_sets[cnt].set_start;
    }

  /* We are done.  No more entries.  */
  return 0;
}
