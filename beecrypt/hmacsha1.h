/*
 * hmacsha1.h
 *
 * HMAC-SHA-1 message authentication code, header
 *
 * Copyright (c) 1999, 2000, 2001 Virtual Unlimited B.V.
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
#include "fips180.h"

typedef struct
{
/*@unused@*/	byte kxi[64];
/*@unused@*/	byte kxo[64];
	sha1Param param;
} hmacsha1Param;

#ifdef __cplusplus
extern "C" {
#endif

/*@unused@*/ extern BEEDLLAPI const keyedHashFunction hmacsha1;

BEEDLLAPI
int hmacsha1Setup (hmacsha1Param* sp, const uint32* key, int keybits)
	/*@modifies sp @*/;
BEEDLLAPI
int hmacsha1Reset (hmacsha1Param* sp)
	/*@modifies sp @*/;
BEEDLLAPI
int hmacsha1Update(hmacsha1Param* sp, const byte* data, int size)
	/*@modifies sp @*/;
BEEDLLAPI
int hmacsha1Digest(hmacsha1Param* sp, uint32* data)
	/*@modifies sp, data @*/;

#ifdef __cplusplus
}
#endif

#endif
