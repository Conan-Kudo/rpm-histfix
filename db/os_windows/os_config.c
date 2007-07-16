/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: os_config.c,v 12.6 2006/08/24 14:46:21 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __os_is_winnt --
 *	Return 1 if Windows/NT, otherwise 0.
 *
 * PUBLIC: int __os_is_winnt __P((void));
 */
int
__os_is_winnt()
{
	static int __os_type = -1;

	/*
	 * The value of __os_type is computed only once, and cached to
	 * avoid the overhead of repeated calls to GetVersion().
	 */
	if (__os_type == -1) {
		if ((GetVersion() & 0x80000000) == 0)
			__os_type = 1;
		else
			__os_type = 0;
	}
	return (__os_type);
}

/*
 * __os_fs_notzero --
 *	Return 1 if allocated filesystem blocks are not zeroed.
 */
int
__os_fs_notzero()
{
	static int __os_notzero = -1;
	OSVERSIONINFO osvi;

	/*
	 * Windows/NT zero-fills pages that were never explicitly written to
	 * the file.  Note however that this is *NOT* documented.  In fact, the
	 * Win32 documentation makes it clear that there are no guarantees that
	 * uninitialized bytes will be zeroed:
	 *
	 *   If the file is extended, the contents of the file between the old
	 *   EOF position and the new position are not defined.
	 *
	 * Experiments confirm that NT/2K/XP all zero fill for both NTFS and
	 * FAT32.  Cygwin also relies on this behavior.  This is the relevant
	 * comment from Cygwin:
	 *
	 *    Oops, this is the bug case - Win95 uses whatever is on the disk
	 *    instead of some known (safe) value, so we must seek back and fill
	 *    in the gap with zeros. - DJ
	 *    Note: this bug doesn't happen on NT4, even though the
	 *    documentation for WriteFile() says that it *may* happen on any OS.
	 *
	 * We're making a bet, here, but we made it a long time ago and haven't
	 * yet seen any evidence that it was wrong.
	 *
	 * Windows 95/98 and On-Time give random garbage, and that breaks
	 * Berkeley DB.
	 *
	 * The value of __os_notzero is computed only once, and cached to
	 * avoid the overhead of repeated calls to GetVersion().
	 */
	if (__os_notzero == -1) {
		if (__os_is_winnt()) {
			osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
			GetVersionEx(&osvi);
			if (_tcscmp(osvi.szCSDVersion, _T("RTTarget-32")) == 0)
				__os_notzero = 1;	/* On-Time */
			else
				__os_notzero = 0;	/* Windows/NT */
		} else
			__os_notzero = 1;		/* Not Windows/NT */
	}
	return (__os_notzero);
}

/*
 * __os_support_direct_io --
 *	Check to see if we support direct I/O.
 */
int
__os_support_direct_io()
{
	return (1);
}

/*
 * __os_support_db_register --
 *	Return 1 if the system supports DB_REGISTER.
 */
int
__os_support_db_register()
{
	return (__os_is_winnt());
}

/*
 * __os_support_replication --
 *	Return 1 if the system supports replication.
 */
int
__os_support_replication()
{
	return (__os_is_winnt());
}
