/** \ingroup MP_m
 * \file mpprime.h
 *
 * Multi-precision primes, header.
 */

/*
 * Copyright (c) 2003 Bob Deblier
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

#ifndef _MPPRIME_H
#define _MPPRIME_H

#include "mpbarrett.h"

#define SMALL_PRIMES_PRODUCT_MAX	64

/**
 */
/*@-exportlocal@*/
extern uint32* mp32spprod[SMALL_PRIMES_PRODUCT_MAX];
/*@=exportlocal@*/

#ifdef __cplusplus
extern "C" {
#endif

/**
 */
BEECRYPTAPI
int  mp32ptrials     (uint32 bits)
	/*@*/;

/**
 */
BEECRYPTAPI
int  mp32pmilrab_w   (const mpbarrett* p, randomGeneratorContext* rc, int t, /*@out@*/ uint32* wksp)
	/*@modifies wksp @*/;

/**
 */
BEECRYPTAPI
void mp32prnd_w      (mpbarrett* p, randomGeneratorContext* rc, uint32 size, int t, /*@null@*/ const mpnumber* f, /*@out@*/ uint32* wksp)
	/*@globals mp32spprod @*/
	/*@modifies p, rc, wksp @*/;

/**
 */
BEECRYPTAPI
void mp32prndsafe_w  (mpbarrett* p, randomGeneratorContext* rc, uint32 size, int t, /*@out@*/ uint32* wksp)
	/*@globals mp32spprod @*/
	/*@modifies p, rc, wksp @*/;

#ifdef	NOTYET
/**
 */
BEECRYPTAPI /*@unused@*/
void mp32prndcon_w   (mpbarrett* p, randomGeneratorContext* rc, uint32, int, const mpnumber*, const mpnumber*, const mpnumber*, mpnumber*, /*@out@*/ uint32* wksp)
	/*@modifies wksp @*/;
#endif

/**
 */
BEECRYPTAPI
void mp32prndconone_w(mpbarrett* p, randomGeneratorContext* rc, uint32 size, int t, const mpbarrett* q, /*@null@*/ const mpnumber* f, mpnumber* r, int cofactor, /*@out@*/ uint32* wksp)
	/*@globals mp32spprod @*/
	/*@modifies p, rc, r, wksp @*/;

#ifdef __cplusplus
}
#endif

#endif
