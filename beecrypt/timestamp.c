/*
 * Copyright (c) 1999, 2000, 2002, 2003 Virtual Unlimited B.V.
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

/*!\file timestamp.c
 * \brief Java compatible 64-bit timestamp.
 * \author Bob Deblier <bob.deblier@pandora.be>
 */

#include "system.h"

#include "timestamp.h"

#include "debug.h"

javalong timestamp()
{
	javalong tmp;
	#if HAVE_SYS_TIME_H
	# if HAVE_GETTIMEOFDAY
	struct timeval now;

	gettimeofday(&now, 0);

	tmp = ((javalong) now.tv_sec) * 1000 + (now.tv_usec / 1000);
	# else
	#  error
	# endif
	#elif HAVE_TIME_H
	tmp = ((javalong) time(0)) * 1000;
	#else
	# error implement other time function
	#endif

	return tmp;
}
