/*
 * Copyright (c) 2001, 2002 Virtual Unlimited B.V.
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

/*!\file dsa.c
 * \brief Digital Signature Algorithm, as specified by NIST FIPS 186.
 *
 * FIPS 186 specifies the DSA algorithm as having a large prime \f$p\f$,
 * a cofactor \f$q\f$ and a generator \f$g\f$ of a subgroup of
 * \f$\mathds{Z}^{*}_p\f$ with order \f$q\f$. The private and public key
 * values are \f$x\f$ and \f$y\f$ respectively.
 *
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup DL_m DL_dsa_m
 *
 *  - Signing equation:
 *   - r = (g^k mod p) mod q and
 *   - s = (inv(k) * (h(m) + x*r)) mod q
 *  - Verifying equation:
 *   - check 0 < r < q and 0 < s < q
 *   - w = inv(s) mod q
 *   - u1 = (h(m)*w) mod q
 *   - u2 = (r*w) mod q
 *   - v = ((g^u1 * y^u2) mod p) mod q
 *   - check v == r
 */

#include "system.h"
#include "dsa.h"
#include "dldp.h"
#include "mp.h"
#include "debug.h"

int dsasign(const mpbarrett* p, const mpbarrett* q, const mpnumber* g, randomGeneratorContext* rgc, const mpnumber* hm, const mpnumber* x, mpnumber* r, mpnumber* s)
{
	register size_t psize = p->size;
	register size_t qsize = q->size;

	register mpw* ptemp;
	register mpw* qtemp;

	register mpw* pwksp;
	register mpw* qwksp;

	register int rc = -1;

	ptemp = (mpw*) malloc((5*psize+2) * sizeof(*ptemp));
	if (ptemp == (mpw*) 0)
		return rc;

	qtemp = (mpw*) malloc((14*qsize+11) * sizeof(*qtemp));
	if (qtemp == (mpw*) 0)
	{
		free(ptemp);
		return rc;
	}

	pwksp = ptemp+psize;
	qwksp = qtemp+3*qsize;

	/* allocate r */
	mpnfree(r);
	mpnsize(r, qsize);

	/* get a random k, invertible modulo q */
	mpbrndinv_w(q, rgc, qtemp, qtemp+qsize, qwksp);

	/* g^k mod p */
	mpbpowmod_w(p, g->size, g->data, qsize, qtemp, ptemp, pwksp);

	/* (g^k mod p) mod q - simple modulo */
	mpnmod(qtemp+2*qsize, psize, ptemp, qsize, q->modl, pwksp);
	mpcopy(qsize, r->data, qtemp+psize+qsize);

	/* allocate s */
	mpnfree(s);
	mpnsize(s, qsize);

	/* x*r mod q */
	mpbmulmod_w(q, x->size, x->data, r->size, r->data, qtemp, qwksp);

	/* add h(m) mod q */
	mpbaddmod_w(q, qsize, qtemp, hm->size, hm->data, qtemp+2*qsize, qwksp);

	/* multiply inv(k) mod q */
	mpbmulmod_w(q, qsize, qtemp+qsize, qsize, qtemp+2*qsize, s->data, qwksp);

	rc = 0;

	free(qtemp);
	free(ptemp);

	return rc;
}

static int _debug = 1;

int dsavrfy(const mpbarrett* p, const mpbarrett* q, const mpnumber* g, const mpnumber* hm, const mpnumber* y, const mpnumber* r, const mpnumber* s)
{
	register size_t psize = p->size;
	register size_t qsize = q->size;

	register mpw* ptemp;
	register mpw* qtemp;

	register mpw* pwksp;
	register mpw* qwksp;

	register int rc = 0;
int xx;

	if (mpz(r->size, r->data))
		return rc;

	if (mpgex(r->size, r->data, qsize, q->modl))
		return rc;

	if (mpz(s->size, s->data))
		return rc;

	if (mpgex(s->size, s->data, qsize, q->modl))
		return rc;

	ptemp = (mpw*) malloc((6*psize+2) * sizeof(*ptemp));
	if (ptemp == (mpw*) 0)
		return rc;

	qtemp = (mpw*) malloc((13*qsize+11) * sizeof(*qtemp));
	if (qtemp == (mpw*) 0)
	{
		free(ptemp);
		return rc;
	}

	pwksp = ptemp+2*psize;
	qwksp = qtemp+2*qsize;

if (_debug) {
fprintf(stderr, "\t                p: "),  mpfprintln(stderr, p->size, p->modl);
fprintf(stderr, "\t                q: "),  mpfprintln(stderr, q->size, q->modl);
fprintf(stderr, "\t                g: "),  mpfprintln(stderr, g->size, g->data);
fprintf(stderr, "\t               hm: "),  mpfprintln(stderr, hm->size, hm->data);
fprintf(stderr, "\t                y: "),  mpfprintln(stderr, y->size, y->data);
fprintf(stderr, "\t                r: "),  mpfprintln(stderr, r->size, r->data);
fprintf(stderr, "\t                s: "),  mpfprintln(stderr, s->size, s->data);
}

	mpsetx(qsize, qtemp+qsize, s->size, s->data);

	/* compute w = inv(s) mod q */
/*@-compdef@*/ /* FIX: mpsetx annotations, qtemp[qsize] is defined */
	if (mpextgcd_w(qsize, qtemp+qsize, q->modl, qtemp, qwksp))
/*@=compdef@*/
	{
if (_debug)
fprintf(stderr, "\t w = inv(s) mod q: "),  mpfprintln(stderr, qsize, qtemp);

		/* compute u1 = h(m)*w mod q */
		mpbmulmod_w(q, hm->size, hm->data, qsize, qtemp, qtemp+qsize, qwksp);

if (_debug)
fprintf(stderr, "\tu1 = h(m)*w mod q: "),  mpfprintln(stderr, qsize, qtemp+qsize);

		/* compute u2 = r*w mod q */
		mpbmulmod_w(q, r->size, r->data, qsize, qtemp, qtemp, qwksp);

if (_debug)
fprintf(stderr, "\tu2 = r*w mod q   : "),  mpfprintln(stderr, qsize, qtemp);

		/* compute g^u1 mod p */
		mpbpowmod_w(p, g->size, g->data, qsize, qtemp+qsize, ptemp, pwksp);

if (_debug)
fprintf(stderr, "\t       g^u1 mod p: "),  mpfprintln(stderr, psize, ptemp);

		/* compute y^u2 mod p */
		mpbpowmod_w(p, y->size, y->data, qsize, qtemp, ptemp+psize, pwksp);

if (_debug)
fprintf(stderr, "\t       y^u2 mod p: "),  mpfprintln(stderr, psize, ptemp+psize);

		/* multiply mod p */
		mpbmulmod_w(p, psize, ptemp, psize, ptemp+psize, ptemp, pwksp);

if (_debug)
fprintf(stderr, "\t   multiply mod p: "),  mpfprintln(stderr, psize, ptemp);

		/* modulo q */
		mpnmod(ptemp+psize, psize, ptemp, qsize, q->modl, pwksp);

if (_debug)
fprintf(stderr, "\tr                : "),  mpfprintln(stderr, r->size, r->data);
if (_debug)
fprintf(stderr, "\tr' mod q         : "),  mpfprintln(stderr, psize, ptemp+psize);

		rc = mpeqx(r->size, r->data, psize, ptemp+psize);
	}

	free(qtemp);
	free(ptemp);

fprintf(stderr, "*** dsavrfy rc %d\n", rc);
	return rc;
}
