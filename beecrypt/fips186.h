/*
 * Copyright (c) 1998, 1999, 2000, 2001, 2002 Virtual Unlimited B.V.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*!\file fips186.h
 * \brief FIPS-186 pseudo-random number generator, headers.
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup PRNG_m PRNG_fips186_m
 */

#ifndef _FIPS186_H
#define _FIPS186_H

#include "beecrypt.h"

#ifdef _REENTRANT
# if WIN32
#  include <windows.h>
#  include <winbase.h>
# else
#  if HAVE_THREAD_H && HAVE_SYNCH_H
#   include <synch.h>
#  elif HAVE_PTHREAD_H
#   include <pthread.h>
#  else
#   error need locking mechanism
#  endif
# endif
#endif

#include "sha1.h"

#if (MP_WBYTES == 8)
# define FIPS186_STATE_SIZE	8
#elif (MP_WBYTES == 4)
# define FIPS186_STATE_SIZE	16
#else
# error
#endif

/*!\ingroup PRNG_fips186_m
 */
typedef struct
{
	#ifdef _REENTRANT
	# if WIN32
	HANDLE			lock;
	# else
	#  if HAVE_THREAD_H && HAVE_SYNCH_H
	mutex_t			lock;
	#  elif HAVE_PTHREAD_H
	pthread_mutex_t	lock;
	#  else
	#   error need locking mechanism
	#  endif
	# endif
	#endif
	sha1Param	param;
	mpw			state[FIPS186_STATE_SIZE];
	byte		digest[20];
	int			digestremain;
} fips186Param;

#ifdef __cplusplus
extern "C" {
#endif

/**
 */
/*@observer@*/ /*@checked@*/
extern BEECRYPTAPI const randomGenerator fips186prng;

/**
 */
/*@-exportlocal@*/
BEECRYPTAPI
int fips186Setup  (fips186Param* fp)
	/*@modifies fp @*/;
/*@=exportlocal@*/

/**
 */
/*@-exportlocal@*/
BEECRYPTAPI
int fips186Seed   (fips186Param* fp, const byte* data, size_t size)
	/*@modifies fp @*/;
/*@=exportlocal@*/

/**
 */
/*@-exportlocal@*/
BEECRYPTAPI
int fips186Next   (fips186Param* fp, byte* data, size_t size)
	/*@modifies fp, data @*/;
/*@=exportlocal@*/

/**
 */
/*@-exportlocal@*/
BEECRYPTAPI
int fips186Cleanup(fips186Param* fp)
	/*@modifies fp @*/;
/*@=exportlocal@*/

#ifdef __cplusplus
}
#endif

#endif
