/*
 * Copyright (c) 2004 Beeyond Software Holding BV
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
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "beecrypt/c++/provider/BeeCryptProvider.h"
#include "beecrypt/c++/provider/DHParameters.h"

using namespace beecrypt::provider;

DHParameters::DHParameters()
{
	_spec = 0;
}

DHParameters::~DHParameters()
{
	if (_spec)
	{
		delete _spec;
		_spec = 0;
	}
}

AlgorithmParameterSpec* DHParameters::engineGetParameterSpec(const type_info& info) throw (InvalidParameterSpecException)
{
	if (info == typeid(AlgorithmParameterSpec) || info == typeid(DHParameterSpec))
	{
		if (_spec)
		{
			return new DHParameterSpec(*_spec);
		}
		else
			throw InvalidParameterSpecException("not initialized");
	}
	else
		throw InvalidParameterSpecException("expected a DHParameterSpec");
}

void DHParameters::engineInit(const AlgorithmParameterSpec& spec) throw (InvalidParameterSpecException)
{
	const DHParameterSpec* tmp = dynamic_cast<const DHParameterSpec*>(&spec);

	if (tmp)
	{
		if (_spec)
		{
			delete _spec;
			_spec = 0;
		}
		_spec = new DHParameterSpec(*tmp);
	}
	else
		throw InvalidParameterSpecException("expected a DHParameterSpec");
}

void DHParameters::engineInit(const byte*, size_t)
{
	throw "not implemented";
}

void DHParameters::engineInit(const byte*, size_t, const String& format)
{
	throw "not implemented";
}
