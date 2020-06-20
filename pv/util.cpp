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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "util.hpp"

#include <extdef.h>

#include <algorithm>
#include <cassert>
#include <sstream>

#include <QDebug>
#include <QTextStream>

using std::fixed;
using std::max;
using std::ostringstream;
using std::setfill;
using std::setprecision;
using std::showpos;
using std::string;

using namespace Qt;

namespace pv {
namespace util {

static QTextStream& operator<<(QTextStream& stream, SIPrefix prefix)
{
	switch (prefix) {
	case SIPrefix::yocto: return stream << 'y';
	case SIPrefix::zepto: return stream << 'z';
	case SIPrefix::atto:  return stream << 'a';
	case SIPrefix::femto: return stream << 'f';
	case SIPrefix::pico:  return stream << 'p';
	case SIPrefix::nano:  return stream << 'n';
	case SIPrefix::micro: return stream << QChar(0x03BC);
	case SIPrefix::milli: return stream << 'm';
	case SIPrefix::kilo:  return stream << 'k';
	case SIPrefix::mega:  return stream << 'M';
	case SIPrefix::giga:  return stream << 'G';
	case SIPrefix::tera:  return stream << 'T';
	case SIPrefix::peta:  return stream << 'P';
	case SIPrefix::exa:   return stream << 'E';
	case SIPrefix::zetta: return stream << 'Z';
	case SIPrefix::yotta: return stream << 'Y';

	default: return stream;
	}
}

int exponent(SIPrefix prefix)
{
	return 3 * (static_cast<int>(prefix) - static_cast<int>(SIPrefix::none));
}

static SIPrefix successor(SIPrefix prefix)
{
	assert(prefix != SIPrefix::yotta);
	return static_cast<SIPrefix>(static_cast<int>(prefix) + 1);
}

// Insert the timestamp value into the stream in fixed-point notation
// (and honor the precision)
static QTextStream& operator<<(QTextStream& stream, const Timestamp& t)
{
	// The multiprecision types already have a function and a stream insertion
	// operator to convert them to a string, however these functions abuse a
	// precision value of zero to print all available decimal places instead of
	// none, and the boost authors refuse to fix this because they don't want
	// to break buggy code that relies on this bug.
	// (https://svn.boost.org/trac/boost/ticket/10103)
	// Therefore we have to work around the case where precision is zero.

	int precision = stream.realNumberPrecision();

	ostringstream ss;
	ss << fixed;

	if (stream.numberFlags() & QTextStream::ForceSign)
		ss << showpos;

	if (0 == precision)
		ss << setprecision(1) << round(t);
	else
		ss << setprecision(precision) << t;

	string str(ss.str());
	if (0 == precision) {
		// remove the separator and the unwanted decimal place
		str.resize(str.size() - 2);
	}

	return stream << QString::fromStdString(str);
}

SIPrefix determine_value_prefix(double v)
{
	SIPrefix prefix;

	if (v == 0) {
		prefix = SIPrefix::none;
	} else {
		int exp = exponent(SIPrefix::yotta);
		prefix = SIPrefix::yocto;
		while ((fabs(v) * pow(10, exp)) > 999 &&
				prefix < SIPrefix::yotta) {
			prefix = successor(prefix);
			exp -= 3;
		}
	}

	return prefix;
}

QString format_time_si(const Timestamp& v, SIPrefix prefix,
	unsigned int precision, QString unit, bool sign)
{
	if (prefix == SIPrefix::unspecified)
		prefix = determine_value_prefix(v.convert_to<double>());

	assert(prefix >= SIPrefix::yocto);
	assert(prefix <= SIPrefix::yotta);

	const Timestamp multiplier = pow(Timestamp(10), -exponent(prefix));

	QString s;
	QTextStream ts(&s);
	if (sign && !v.is_zero())
		ts.setNumberFlags(ts.numberFlags() | QTextStream::ForceSign);
	ts << qSetRealNumberPrecision(precision) << (v * multiplier);
	ts << ' ' << prefix << unit;

	return s;
}

QString format_value_si(double v, SIPrefix prefix, unsigned precision,
	QString unit, bool sign)
{
	if (prefix == SIPrefix::unspecified) {
		prefix = determine_value_prefix(v);

		const int prefix_order = -exponent(prefix);
		precision = (prefix >= SIPrefix::none) ? max((int)(precision + prefix_order), 0) :
			max((int)(precision - prefix_order), 0);
	}

	assert(prefix >= SIPrefix::yocto);
	assert(prefix <= SIPrefix::yotta);

	const double multiplier = pow(10.0, -exponent(prefix));

	QString s;
	QTextStream ts(&s);
	if (sign && (v != 0))
		ts.setNumberFlags(ts.numberFlags() | QTextStream::ForceSign);
	ts.setRealNumberNotation(QTextStream::FixedNotation);
	ts.setRealNumberPrecision(precision);
	ts << (v * multiplier) << ' ' << prefix << unit;

	return s;
}

QString format_time_si_adjusted(const Timestamp& t, SIPrefix prefix,
	unsigned precision, QString unit, bool sign)
{
	// The precision is always given without taking the prefix into account
	// so we need to deduct the number of decimals the prefix might imply
	const int prefix_order = -exponent(prefix);

	const unsigned int relative_prec =
		(prefix >= SIPrefix::none) ? precision :
		max((int)(precision - prefix_order), 0);

	return format_time_si(t, prefix, relative_prec, unit, sign);
}

// Helper for 'format_time_minutes()'.
static QString pad_number(unsigned int number, int length)
{
	return QString("%1").arg(number, length, 10, QChar('0'));
}

QString format_time_minutes(const Timestamp& t, signed precision, bool sign)
{
	const Timestamp whole_seconds = floor(abs(t));
	const Timestamp days = floor(whole_seconds / (60 * 60 * 24));
	const unsigned int hours = fmod(whole_seconds / (60 * 60), 24).convert_to<uint>();
	const unsigned int minutes = fmod(whole_seconds / 60, 60).convert_to<uint>();
	const unsigned int seconds = fmod(whole_seconds, 60).convert_to<uint>();

	QString s;
	QTextStream ts(&s);

	if (t < 0)
		ts << "-";
	else if (sign)
		ts << "+";

	bool use_padding = false;

	// DD
	if (days) {
		ts << days.str().c_str() << ":";
		use_padding = true;
	}

	// HH
	if (hours || days) {
		ts << pad_number(hours, use_padding ? 2 : 0) << ":";
		use_padding = true;
	}

	// MM
	ts << pad_number(minutes, use_padding ? 2 : 0);

	ts << ":";

	// SS
	ts << pad_number(seconds, 2);

	if (precision) {
		ts << ".";

		const Timestamp fraction = fabs(t) - whole_seconds;

		ostringstream ss;
		ss << fixed << setprecision(precision) << setfill('0') << fraction;
		string fs = ss.str();

		// Copy all digits, inserting spaces as unit separators
		for (int i = 1; i <= precision; i++) {
			// Start at index 2 to skip the "0." at the beginning
			ts << fs.at(1 + i);

			if ((i > 0) && (i % 3 == 0) && (i != precision))
				ts << " ";
		}
	}

	return s;
}

/**
 * Split a string into tokens at occurences of the separator.
 *
 * @param[in] text The input string to split.
 * @param[in] separator The delimiter between tokens.
 *
 * @return A vector of broken down tokens.
 */
vector<string> split_string(string text, string separator)
{
	vector<string> result;
	size_t pos;

	while ((pos = text.find(separator)) != std::string::npos) {
		result.push_back(text.substr(0, pos));
		text = text.substr(pos + separator.length());
	}
	result.push_back(text);

	return result;
}

/**
 * Return the width of a string in a given font.
 *
 * @param[in] metric metrics of the font
 * @param[in] string the string whose width should be determined
 *
 * @return width of the string in pixels
 */
std::streamsize text_width(const QFontMetrics &metric, const QString &string)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
	return metric.horizontalAdvance(string);
#else
	return metric.width(string);
#endif
}

} // namespace util
} // namespace pv
