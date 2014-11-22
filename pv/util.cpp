/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "util.hpp"

#include <extdef.h>

#include <assert.h>

#include <QTextStream>
#include <QDebug>

using namespace Qt;

namespace pv {
namespace util {

static const QString SIPrefixes[9] =
	{"f", "p", "n", QChar(0x03BC), "m", "", "k", "M", "G"};
const int FirstSIPrefixPower = -15;

QString format_time(double t, unsigned int prefix,
	unsigned int precision, bool sign)
{
	assert(prefix < countof(SIPrefixes));

	const double multiplier = pow(10.0,
		(int)- prefix * 3 - FirstSIPrefixPower);

	QString s;
	QTextStream ts(&s);
	if (sign) {
		ts << forcesign;
	}
	ts << fixed << qSetRealNumberPrecision(precision)
		<< (t  * multiplier) << SIPrefixes[prefix] << "s";

	return s;
}

QString format_second(double second)
{
	unsigned int i = 0;
	int exp = - FirstSIPrefixPower;

	while ((second * pow(10.0, exp)) > 999.0 && i < countof(SIPrefixes) - 1) {
		i++;
		exp -= 3;
	}

	return format_time(second, i, 0, false);
}

} // namespace util
} // namespace pv
