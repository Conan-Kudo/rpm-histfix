/* Return signed constant represented by attribute.
   Copyright (C) 2000, 2002 Red Hat, Inc.
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
dwarf_formsdata (attr, return_sval, error)
     Dwarf_Attribute attr;
     Dwarf_Signed *return_sval;
     Dwarf_Error *error;
{
  Dwarf_Signed u128;

  switch (attr->form)
    {
    case DW_FORM_data1:
      *return_sval = *attr->valp;
      break;

    case DW_FORM_data2:
      *return_sval = read_2ubyte_unaligned (attr->cu->dbg, attr->valp);
      break;

    case DW_FORM_data4:
      *return_sval = read_4ubyte_unaligned (attr->cu->dbg, attr->valp);
      break;

    case DW_FORM_data8:
      *return_sval = read_8ubyte_unaligned (attr->cu->dbg, attr->valp);
      break;

    case DW_FORM_sdata:
      {
	Dwarf_Small *attrp = attr->valp;
	get_sleb128 (u128, attrp);
	*return_sval = u128;
      }
      break;

    case DW_FORM_udata:
      {
	Dwarf_Small *attrp = attr->valp;
	get_uleb128 (u128, attrp);
	*return_sval = u128;
      }
      break;

    default:
      __libdwarf_error (attr->cu->dbg, error, DW_E_NO_CONSTANT);
      return DW_DLV_ERROR;
    }

  return DW_DLV_OK;
}
