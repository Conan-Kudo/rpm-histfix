/*
 * testdldp.c
 *
 * Unit test program for discrete logarithm domain parameters (over a prime field),
 * as specified by IEEE P.1363.
 *
 * Copyright (c) 2002, 2003 Bob Deblier
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

#include <stdio.h>

#include "beecrypt.h"
#include "dldp.h"

int main()
{
	int failures = 0;

	dldp_p params;
	randomGeneratorContext rngc;

        memset(&params, 0, sizeof(dldp_p));
        memset(&rngc, 0, sizeof(rngc));

        if (randomGeneratorContextInit(&rngc, randomGeneratorDefault()) == 0)
        {                        
                mpnumber gq;

                mpnzero(&gq);

		/* make parameters with p = 512 bits, q = 160 bits, g of order (q) */
                dldp_pgoqMake(&params, &rngc, 512 >> 5, 160 >> 5, 1);

                /* we have the parameters, now see if g^q == 1 */
                mpbnpowmod(&params.p, &params.g, (mpnumber*) &params.q, &gq);
                if (mp32isone(gq.size, gq.data))
			printf("ok\n");
		else
			failures++;

                mpnfree(&gq);

                dldp_pFree(&params);

                randomGeneratorContextFree(&rngc);  
        }
	else
		return -1;

	return failures;
}
