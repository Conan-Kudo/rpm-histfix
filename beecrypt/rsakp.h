/** \ingroup RSA_m
 * \file rsakp.h
 *
 * RSA Keypair, header.
 */

/*
 * <conformance statement for IEEE P1363 needed here>
 *
 * Copyright (c) 2000, 2001, 2002 Virtual Unlimited B.V.
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

#ifndef _RSAKP_H
#define _RSAKP_H

#include "rsapk.h"

typedef struct
{
	mp32barrett n;
	mpnumber e;
	mpnumber d;
	mp32barrett p;
	mp32barrett q;
	mpnumber d1;
	mpnumber d2;
	mpnumber c;
} rsakp;

#ifdef __cplusplus
extern "C" {
#endif

/**
 */
BEECRYPTAPI /*@unused@*/
int rsakpMake(rsakp* kp, randomGeneratorContext* rgc, int nsize)
	/*@modifies kp, rgc @*/;

/**
 */
BEECRYPTAPI /*@unused@*/
int rsakpInit(rsakp* kp)
	/*@modifies kp @*/;

/**
 */
BEECRYPTAPI /*@unused@*/
int rsakpFree(rsakp* kp)
	/*@modifies kp @*/;

/**
 */
BEECRYPTAPI /*@unused@*/
int rsakpCopy(rsakp* dst, const rsakp* src)
	/*@modifies dst @*/;

#ifdef __cplusplus
}
#endif

#endif
