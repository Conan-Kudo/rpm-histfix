/** \ingroup HMAC_sha1_m HMAC_m
 * \file hmacsha1.h
 *
 * HMAC-SHA-1 message authentication code, header.
 */

/*
 * Copyright (c) 1999, 2000, 2001, 2002 Virtual Unlimited B.V.
 *
 * Author: Bob Deblier <bob@virtualunlimited.com>
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

#ifndef _HMACSHA1_H
#define _HMACSHA1_H

#include "hmac.h"
#include "sha1.h"

/** \ingroup HMAC_sha1_m
 */
typedef struct
{
	sha1Param sparam;
	byte kxi[64];
	byte kxo[64];
} hmacsha1Param;

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup HMAC_sha1_m
 */
/*@observer@*/ /*@checked@*/
extern BEECRYPTAPI const keyedHashFunction hmacsha1;

/** \ingroup HMAC_sha1_m
 */
/*@-exportlocal@*/
BEECRYPTAPI
int hmacsha1Setup (hmacsha1Param* sp, const byte* key, size_t keybits)
	/*@globals sha1 @*/
	/*@modifies sp @*/;
/*@=exportlocal@*/

/** \ingroup HMAC_sha1_m
 */
/*@-exportlocal@*/
BEECRYPTAPI
int hmacsha1Reset (hmacsha1Param* sp)
	/*@globals sha1 @*/
	/*@modifies sp @*/;
/*@=exportlocal@*/

/** \ingroup HMAC_sha1_m
 */
/*@-exportlocal@*/
BEECRYPTAPI
int hmacsha1Update(hmacsha1Param* sp, const byte* data, size_t size)
	/*@globals sha1 @*/
	/*@modifies sp @*/;
/*@=exportlocal@*/

/** \ingroup HMAC_sha1_m
 */
/*@-exportlocal@*/
BEECRYPTAPI
int hmacsha1Digest(hmacsha1Param* sp, byte* data)
	/*@globals sha1 @*/
	/*@modifies sp, data @*/;
/*@=exportlocal@*/

#ifdef __cplusplus
}
#endif

#endif
