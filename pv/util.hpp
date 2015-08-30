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

#ifndef PULSEVIEW_UTIL_HPP
#define PULSEVIEW_UTIL_HPP

#include <cmath>

#include <boost/multiprecision/cpp_dec_float.hpp>

#include <QMetaType>
#include <QString>

namespace pv {
namespace util {

enum class TimeUnit {
	Time = 1,
	Samples = 2
};

enum class SIPrefix {
	unspecified = -1,
	yocto, zepto,
	atto, femto, pico,
	nano, micro, milli,
	none,
	kilo, mega, giga,
	tera, peta, exa,
	zetta, yotta
};

/// Returns the exponent that corresponds to a given prefix.
int exponent(SIPrefix prefix);

/// Timestamp type providing yoctosecond resolution.
typedef boost::multiprecision::number<
	boost::multiprecision::cpp_dec_float<24>,
	boost::multiprecision::et_off> Timestamp;

/**
 * Formats a given value with the specified SI prefix.
 * @param v The value to format.
 * @param unit The unit of quantity.
 * @param prefix The prefix to use.
 * @param precision The number of digits after the decimal separator.
 * @param sign Whether or not to add a sign also for positive numbers.
 *
 * @return The formated value.
 */
QString format_si_value(
	const Timestamp& v, QString unit, SIPrefix prefix = SIPrefix::unspecified,
	unsigned precision = 0, bool sign = true);

/**
 * Formats a given time with the specified SI prefix.
 * @param t The time value in seconds to format.
 * @param prefix The prefix to use.
 * @param unit The unit of quantity.
 * @param precision The number of digits after the decimal separator or period (.).
 *
 * @return The formated value.
 */
QString format_time(
	const Timestamp& t, SIPrefix prefix = SIPrefix::unspecified,
	TimeUnit unit = TimeUnit::Time, unsigned precision = 0);

/**
 * Formats a given time value with a SI prefix so that the
 * value is between 1 and 999.
 * @param second The time value in seconds to format.
 *
 * @return The formated value.
 */
QString format_second(const Timestamp& second);

} // namespace util
} // namespace pv

Q_DECLARE_METATYPE(pv::util::Timestamp)

#endif // PULSEVIEW_UTIL_HPP
