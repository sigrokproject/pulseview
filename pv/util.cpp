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

static const QString SIPrefixes[17] =
	{"y", "z", "a", "f", "p", "n", QChar(0x03BC), "m", "", "k", "M", "G",
	"T", "P", "E", "Z", "Y"};
const int FirstSIPrefixPower = -24;

QString format_si_value(double v, QString unit, int prefix,
	unsigned int precision, bool sign)
{
	if (prefix < 0) {
		int exp = -FirstSIPrefixPower;

		prefix = 0;
		while ((fabs(v) * pow(10.0, exp)) > 999.0 &&
			prefix < (int)(countof(SIPrefixes) - 1)) {
			prefix++;
			exp -= 3;
		}
	}

	assert(prefix >= 0);
	assert(prefix < (int)countof(SIPrefixes));

	const double multiplier = pow(10.0,
		(int)- prefix * 3 - FirstSIPrefixPower);

	QString s;
	QTextStream ts(&s);
	if (sign)
		ts << forcesign;
	ts << fixed << qSetRealNumberPrecision(precision) <<
		(v  * multiplier) << " " << SIPrefixes[prefix] << unit;

	return s;
}

QString format_time(double t, int prefix, TimeUnit unit,
	unsigned int precision, bool sign)
{
	if (unit == TimeUnit::Time)
		return format_si_value(t, "s", prefix, precision, sign);
	else
		return format_si_value(t, "sa", prefix, precision, sign);
}

QString format_second(double second)
{
	return format_si_value(second, "s", -1, 0, false);
}

} // namespace util
} // namespace pv
