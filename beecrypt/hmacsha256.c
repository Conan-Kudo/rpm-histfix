/*
 * Copyright (c) 2000, 2001, 2002 Virtual Unlimited B.V.
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

/*!\file hmacsha256.c
 * \brief HMAC-SHA-256 message digest algorithm.
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup HMAC_m HMAC_sha256_m
 */

#include "system.h"
#include "hmacsha256.h"
#include "debug.h"

/*!\addtogroup HMAC_sha256_m
 * \{
 */

/*@-sizeoftype@*/
const keyedHashFunction hmacsha256 = {
	"HMAC-SHA-256",
	sizeof(hmacsha256Param),
	64U,
	8U * sizeof(uint32_t),
	64U,
	512U,
	32U,
	(keyedHashFunctionSetup) hmacsha256Setup,
	(keyedHashFunctionReset) hmacsha256Reset,
	(keyedHashFunctionUpdate) hmacsha256Update,
	(keyedHashFunctionDigest) hmacsha256Digest
};
/*@=sizeoftype@*/

/*@-type@*/	/* fix: cast to (hashFunctionParam*) */
int hmacsha256Setup (hmacsha256Param* sp, const byte* key, size_t keybits)
{
	return hmacSetup(sp->kxi, sp->kxo, &sha256, &sp->sparam, key, keybits);
}

int hmacsha256Reset (hmacsha256Param* sp)
{
	return hmacReset(sp->kxi, &sha256, &sp->sparam);
}

int hmacsha256Update(hmacsha256Param* sp, const byte* data, size_t size)
{
	return hmacUpdate(&sha256, &sp->sparam, data, size);
}

int hmacsha256Digest(hmacsha256Param* sp, byte* data)
{
	return hmacDigest(sp->kxo, &sha256, &sp->sparam, data);
}
/*@=type@*/

/*!\}
 */
