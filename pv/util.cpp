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

#include <algorithm>

#include <QTextStream>
#include <QDebug>

using namespace Qt;

namespace pv {
namespace util {

static const QString SIPrefixes[17] =
	{"y", "z", "a", "f", "p", "n", QChar(0x03BC), "m", "", "k", "M", "G",
	"T", "P", "E", "Z", "Y"};
const int FirstSIPrefix = 8;
const int FirstSIPrefixPower = -(FirstSIPrefix * 3);

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

static QString pad_number(unsigned int number, int length)
{
	return QString("%1").arg(number, length, 10, QChar('0'));
}

static QString format_time_in_full(double t, signed precision, bool force_sign)
{
	const unsigned int whole_seconds = abs((int) t);
	const unsigned int days = whole_seconds / (60 * 60 * 24);
	const unsigned int hours = (whole_seconds / (60 * 60)) % 24;
	const unsigned int minutes = (whole_seconds / 60) % 60;
	const unsigned int seconds = whole_seconds % 60;

	QString s;
	QTextStream ts(&s);

	if (force_sign && (t >= 0))
		ts << "+";
	if (t < 0)
		ts << "-";

	bool use_padding = false;

	if (days) {
		ts << days << ":";
		use_padding = true;
	}

	if (hours || days) {
		ts << pad_number(hours, use_padding ? 2 : 0) << ":";
		use_padding = true;
	}

	if (minutes || hours || days) {
		ts << pad_number(minutes, use_padding ? 2 : 0);
		use_padding = true;

		// We're not showing any seconds with a negative precision
		if (precision >= 0)
			 ts << ":";
	}

	// precision < 0: Use DD:HH:MM format
	// precision = 0: Use DD:HH:MM:SS format
	// precision > 0: Use DD:HH:MM:SS.mmm nnn ppp fff format
	if (precision >= 0) {
		ts << pad_number(seconds, use_padding ? 2 : 0);

		const double fraction = fabs(t - whole_seconds);

		if (precision > 0 && precision < 1000) {
			QString fs = QString("%1").arg(fraction, -(2 + precision), 'f',
				precision, QChar('0'));

			ts << ".";

			// Copy all digits, inserting spaces as unit separators
			for (int i = 1; i <= precision; i++) {
				// Start at index 2 to skip the "0." at the beginning
				ts << fs[1 + i];

				if ((i > 0) && (i % 3 == 0))
					ts << " ";
			}
		}
	}

	return s;
}

static QString format_time_with_si(double t, QString unit, int prefix,
	unsigned int precision, bool sign)
{
	// The precision is always given without taking the prefix into account
	// so we need to deduct the number of decimals the prefix might imply
	const int prefix_order =
		-(prefix * 3 + pv::util::FirstSIPrefixPower);

	const unsigned int relative_prec =
		(prefix >= pv::util::FirstSIPrefix) ? precision :
		std::max((int)(precision - prefix_order), 0);

	return format_si_value(t, unit, prefix, relative_prec, sign);
}

QString format_time(double t, int prefix, TimeUnit unit,
	unsigned int precision, double step_size, bool sign)
{
	// If we have to use samples then we have no alternative formats
	if (unit == Samples)
		return format_time_with_si(t, "sa", prefix, precision, sign);

	// View zoomed way out -> low precision (0), high step size (>=60s)
	// -> DD:HH:MM
	if ((precision == 0) && (step_size >= 60))
		return format_time_in_full(t, -1, sign);

	// View in "normal" range -> medium precision, medium step size
	// -> HH:MM:SS.mmm... or xxxx (si unit) if less than 60 seconds
	// View zoomed way in -> high precision (>3), low step size (<1s)
	// -> HH:MM:SS.mmm... or xxxx (si unit) if less than 60 seconds
	if (abs(t) < 60)
		return format_time_with_si(t, "s", prefix, precision, sign);
	else
		return format_time_in_full(t, precision, sign);
}

QString format_second(double second)
{
	return format_si_value(second, "s", -1, 0, false);
}

} // namespace util
} // namespace pv
