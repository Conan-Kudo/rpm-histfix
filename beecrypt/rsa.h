/*
 * Copyright (c) 2000, 2002 Virtual Unlimited B.V.
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

/*!\file rsa.h
 * \brief RSA algorithm, headers.
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup IF_m IF_rsa_m
 */

#ifndef _RSA_H
#define _RSA_H

#include "rsakp.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The raw RSA public key operation.
 *
 * This function can be used for encryption and verifying.
 *
 * It performs the following operation:
 * \li \f$c=m^{e}\ \textrm{mod}\ n\f$
 *
 * @param pk		RSA public key
 * @param m		message
 * @param c		ciphertext
 * @retval		0 on success, -1 on failure
 */ 
BEECRYPTAPI /*@unused@*/
 int rsapub(const rsapk* pk, const mpnumber* m, mpnumber* c)
	/*@modifies c @*/;

/**
 * The raw RSA private key operation.
 *
 * This function can be used for decryption and signing.
 *
 * It performs the operation:
 * \li \f$m=c^{d}\ \textrm{mod}\ n\f$
 *
 * @param kp		RSA keypair
 * @param c		ciphertext
 * @param m		message
 * @retval		0 on success, -1 on failure
 */
BEECRYPTAPI /*@unused@*/
int rsapri   (const rsakp* kp, const mpnumber* c, mpnumber* m)
	/*@modifies m @*/;

/**
 * The raw RSA private key operation, with Chinese Remainder Theorem.
 *
 * It performs the operation:
 * \li \f$j_1=c^{d_1}\ \textrm{mod}\ p\f$
 * \li \f$j_2=c^{d_2}\ \textrm{mod}\ q\f$
 * \li \f$h=c \cdot (j_1-j_2)\ \textrm{mod}\ p\f$
 * \li \f$m=j_2+hq\f$
 *
 * @param kp		RSA keypair
 * @param c		ciphertext
 * @param m		message
 * @retval		0 on success, -1 on failure.
 */
BEECRYPTAPI
int rsapricrt(const rsakp* kp, const mpnumber* c, mpnumber* m)
	/*@modifies m @*/;

/**
 * Verify if ciphertext \e c was encrypted from cleartext \e m
 * with the private key matching the given public key \e pk.
 *
 * @warning The return type of this function should be a boolean, but since
 *          that type isn't as portable, an int is used.
 *
 * @param pk		RSA public key
 * @param m		cleartext message
 * @param c		ciphertext message
 * @retval		1 on success, 0 on failure
 */
BEECRYPTAPI
int rsavrfy  (const rsapk* pk, const mpnumber* m, const mpnumber* c)
	/*@*/;

#ifdef __cplusplus
}
#endif

#endif
