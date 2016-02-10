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

#ifndef Q_MOC_RUN
#include <boost/multiprecision/cpp_dec_float.hpp>
#endif

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
 * Formats a given timestamp with the specified SI prefix.
 *
 * If 'prefix' is left 'unspecified', the function chooses a prefix so that
 * the value in front of the decimal point is between 1 and 999.
 *
 * The default value "s" for the unit argument makes the most sense when
 * formatting time values, but a different value can be given if the function
 * is reused to format a value of another quantity.
 *
 * @param t The value to format.
 * @param prefix The SI prefix to use.
 * @param precision The number of digits after the decimal separator.
 * @param unit The unit of quantity.
 * @param sign Whether or not to add a sign also for positive numbers.
 *
 * @return The formatted value.
 */
QString format_time_si(
	const Timestamp& v,
	SIPrefix prefix = SIPrefix::unspecified,
	unsigned precision = 0,
	QString unit = "s",
	bool sign = true);

/**
 * Wrapper around 'format_time_si()' that interprets the given 'precision'
 * value as the number of decimal places if the timestamp would be formatted
 * without a SI prefix (using 'SIPrefix::none') and adjusts the precision to
 * match the given 'prefix'
 *
 * @param t The value to format.
 * @param prefix The SI prefix to use.
 * @param precision The number of digits after the decimal separator if the
 *        'prefix' would be 'SIPrefix::none', see above for more information.
 * @param unit The unit of quantity.
 * @param sign Whether or not to add a sign also for positive numbers.
 *
 * @return The formatted value.
 */
QString format_time_si_adjusted(
	const Timestamp& t,
	SIPrefix prefix,
	unsigned precision = 0,
	QString unit = "s",
	bool sign = true);

/**
 * Formats the given timestamp using "[+-]DD:HH:MM:SS.mmm uuu nnn ppp..." format.
 *
 * "DD" and "HH" are left out if they would be "00" (but if "DD" is generated,
 * "HH" is also always generated. The "MM:SS" part is always produced, the
 * number of subsecond digits can be influenced using the 'precision' parameter.
 *
 * @param t The value to format.
 * @param precision The number of digits after the decimal separator.
 * @param sign Whether or not to add a sign also for positive numbers.
 *
 * @return The formatted value.
 */
QString format_time_minutes(
	const Timestamp& t,
	signed precision = 0,
	bool sign = true);

} // namespace util
} // namespace pv

Q_DECLARE_METATYPE(pv::util::Timestamp)

#endif // PULSEVIEW_UTIL_HPP
