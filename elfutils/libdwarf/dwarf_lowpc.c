/* Return low program counter value associated with die.
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

#include <dwarf.h>

#include <libdwarfP.h>


int
dwarf_lowpc (die, return_lowpc, error)
     Dwarf_Die die;
     Dwarf_Addr *return_lowpc;
     Dwarf_Error *error;
{
  Dwarf_Debug dbg = die->cu->dbg;
  Dwarf_Small *die_addr;
  Dwarf_Word u128;
  Dwarf_Abbrev abbrev;
  Dwarf_Small *attrp;

  /* Address of the given die.  */
  die_addr = die->addr;

  /* Get abbrev code.  */
  get_uleb128 (u128, die_addr);
  /*  And get the abbreviation itself.  */
  abbrev = __libdwarf_get_abbrev (dbg, die->cu, u128, error);
  if (abbrev == NULL)
    return DW_DLV_ERROR;

  /* This is where the attributes start.  */
  attrp = abbrev->attrp;

  /* Search the name attribute.  */
  while (1)
    {
      Dwarf_Word attr_name;
      Dwarf_Word attr_form;

      /* Are we still in bounds?  */
      if (unlikely (attrp
		    >= ((Dwarf_Small *) dbg->sections[IDX_debug_abbrev].addr
			+ dbg->sections[IDX_debug_abbrev].size)))
	{
	  __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
	  return DW_DLV_ERROR;
	}

      /* Get attribute name and form.

	 XXX We don't check whether this reads beyond the end of the
	 section.  */
      get_uleb128 (attr_name, attrp);
      get_uleb128 (attr_form, attrp);

      /* We can stop if we found the attribute with value zero.  */
      if (attr_name == 0 && attr_form == 0)
	break;

      /* Is this the name attribute?  */
      if (unlikely (attr_name == DW_AT_low_pc))
	{
	  if (unlikely (attr_form != DW_FORM_addr))
	    {
	      __libdwarf_error (dbg, error, DW_E_INVALID_DWARF);
	      return DW_DLV_ERROR;
	    }

	  if (die->cu->address_size == 4)
	    *return_lowpc = read_4ubyte_unaligned (dbg, die_addr);
	  else
	    *return_lowpc = read_8ubyte_unaligned (dbg, die_addr);

	  return DW_DLV_OK;
	}

      /* Skip over the rest of this attribute (if there is any).  */
      if (attr_form != 0)
	{
	  size_t len;

	  if (unlikely (__libdwarf_form_val_len (dbg, die->cu, attr_form,
						 die_addr, &len, error)
			!= DW_DLV_OK))
	    return DW_DLV_ERROR;

	  die_addr += len;
	}
    }

  /* No such attribute present.  */
  return DW_DLV_NO_ENTRY;
}
