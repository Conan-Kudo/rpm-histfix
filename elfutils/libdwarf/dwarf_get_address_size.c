/* Return address size.
   Copyright (C) 2000, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2000.

   This program is Open Source software; you can redistribute it and/or
   modify it under the terms of the Open Software License version 1.0 as
   published by the Open Source Initiative.

   You should have received a copy of the Open Software License along
   with this program; if not, you may obtain a copy of the Open Software
   License version 1.0 from http://www.opensource.org/licenses/osl.php or
   by writing the Open Source Initiative c/o Lawrence Rosen, Esq.,
   3001 King Ranch Road, Ukiah, CA 95482.   */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <libdwarfP.h>


int
dwarf_get_address_size (dbg, addr_size, error)
     Dwarf_Debug dbg;
     Dwarf_Half *addr_size;
     Dwarf_Error *error;
{
  *addr_size = elf_getident (dbg->elf, NULL)[EI_CLASS] == ELFCLASS64 ? 8 : 4;
  return DW_DLV_OK;
}
