/*
 * Copyright (c) 1999, 2000, 2002 Virtual Unlimited B.V.
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

/*!\file blowfish.h
 * \brief Blowfish block cipher.
 *
 * For more information on this blockcipher, see:
 * "Applied Cryptography", second edition
 *  Bruce Schneier
 *  Wiley & Sons
 *
 * Also see http://www.counterpane.com/blowfish.html
 *
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup BC_m BC_blowfish_m
 */

#ifndef _BLOWFISH_H
#define _BLOWFISH_H

#include "beecrypt.h"
#include "blowfishopt.h"

#define BLOWFISHROUNDS	16
#define BLOWFISHPSIZE	(BLOWFISHROUNDS+2)

/** \ingroup BC_blowfish_m
 */
typedef struct
{
	uint32_t p[BLOWFISHPSIZE];
	uint32_t s[1024];
	uint32_t fdback[2];
} blowfishParam;

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup BC_blowfish_m
 */
/*@observer@*/ /*@checked@*/
extern const BEECRYPTAPI blockCipher blowfish;

/** \ingroup BC_blowfish_m
 */
/*@-exportlocal@*/
BEECRYPTAPI
int blowfishSetup  (blowfishParam* bp, const byte* key, size_t keybits, cipherOperation op)
	/*@modifies bp @*/;
/*@=exportlocal@*/

/** \ingroup BC_blowfish_m
 */
/*@-exportlocal@*/
BEECRYPTAPI
int blowfishSetIV  (blowfishParam* bp, const byte* iv)
	/*@modifies bp @*/;
/*@=exportlocal@*/

/** \ingroup BC_blowfish_m
 */
/*@-exportlocal@*/
BEECRYPTAPI
int blowfishEncrypt(blowfishParam* bp, uint32_t* dst, const uint32_t* src)
	/*@modifies bp, dst @*/;
/*@=exportlocal@*/

/** \ingroup BC_blowfish_m
 */
/*@-exportlocal@*/
BEECRYPTAPI
int blowfishDecrypt(blowfishParam* bp, uint32_t* dst, const uint32_t* src)
	/*@modifies bp, dst @*/;
/*@=exportlocal@*/

/** \ingroup BC_blowfish_m
 */
/*@-exportlocal@*/
BEECRYPTAPI /*@observer@*/
uint32_t* blowfishFeedback(blowfishParam* bp)
	/*@*/;
/*@=exportlocal@*/

#ifdef __cplusplus
}
#endif

#endif
