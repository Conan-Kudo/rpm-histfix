/*
nextscn.c - implementation of the elf_nextscn(3) function.
Copyright (C) 1995, 1996 Michael Riepe <michael@stud.uni-hannover.de>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <private.h>

Elf_Scn*
elf_nextscn(Elf *elf, Elf_Scn *scn) {
    if (!elf) {
	return NULL;
    }
    elf_assert(elf->e_magic == ELF_MAGIC);
    if (scn) {
	elf_assert(scn->s_magic == SCN_MAGIC);
	if (scn->s_elf == elf) {
	    return scn->s_link;
	}
	seterr(ERROR_ELFSCNMISMATCH);
    }
    else if (elf->e_kind != ELF_K_ELF) {
	seterr(ERROR_NOTELF);
    }
    else if (elf->e_ehdr || _elf_cook(elf)) {
	elf_assert(elf->e_ehdr);
	for (scn = elf->e_scn_1; scn; scn = scn->s_link) {
	    elf_assert(scn->s_magic == SCN_MAGIC);
	    elf_assert(scn->s_elf == elf);
	    if (scn->s_index == 1) {
		return scn;
	    }
	}
	seterr(ERROR_NOSUCHSCN);
    }
    return NULL;
}
