/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2015 Jens Steinhauser <jens.steinhauser@gmail.com>
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

#include <boost/test/unit_test.hpp>

#include "pv/util.hpp"
#include "test/test.hpp"

using namespace pv::util;
using ts = pv::util::Timestamp;

using std::bind;

namespace {
	QChar mu = QChar(0x03BC);

	pv::util::SIPrefix unspecified = pv::util::SIPrefix::unspecified;
	pv::util::SIPrefix yocto       = pv::util::SIPrefix::yocto;
	pv::util::SIPrefix nano        = pv::util::SIPrefix::nano;
/*	pv::util::SIPrefix micro       = pv::util::SIPrefix::micro; // Not currently used */
	pv::util::SIPrefix milli       = pv::util::SIPrefix::milli;
	pv::util::SIPrefix none        = pv::util::SIPrefix::none;
	pv::util::SIPrefix kilo        = pv::util::SIPrefix::kilo;
	pv::util::SIPrefix yotta       = pv::util::SIPrefix::yotta;

/*	pv::util::TimeUnit Time = pv::util::TimeUnit::Time; // Not currently used */
}  // namespace

BOOST_AUTO_TEST_SUITE(UtilTest)

BOOST_AUTO_TEST_CASE(exponent_test)
{
	BOOST_CHECK_EQUAL(exponent(SIPrefix::yocto), -24);
	BOOST_CHECK_EQUAL(exponent(SIPrefix::zepto), -21);
	BOOST_CHECK_EQUAL(exponent(SIPrefix::atto),  -18);
	BOOST_CHECK_EQUAL(exponent(SIPrefix::femto), -15);
	BOOST_CHECK_EQUAL(exponent(SIPrefix::pico),  -12);
	BOOST_CHECK_EQUAL(exponent(SIPrefix::nano),   -9);
	BOOST_CHECK_EQUAL(exponent(SIPrefix::micro),  -6);
	BOOST_CHECK_EQUAL(exponent(SIPrefix::milli),  -3);
	BOOST_CHECK_EQUAL(exponent(SIPrefix::none),    0);
	BOOST_CHECK_EQUAL(exponent(SIPrefix::kilo),    3);
	BOOST_CHECK_EQUAL(exponent(SIPrefix::mega),    6);
	BOOST_CHECK_EQUAL(exponent(SIPrefix::giga),    9);
	BOOST_CHECK_EQUAL(exponent(SIPrefix::tera),   12);
	BOOST_CHECK_EQUAL(exponent(SIPrefix::peta),   15);
	BOOST_CHECK_EQUAL(exponent(SIPrefix::exa),    18);
	BOOST_CHECK_EQUAL(exponent(SIPrefix::zetta),  21);
	BOOST_CHECK_EQUAL(exponent(SIPrefix::yotta),  24);
}

BOOST_AUTO_TEST_CASE(format_time_si_test)
{
	// check prefix calculation

	BOOST_CHECK_EQUAL(format_time_si(ts("0")), "0 s");

	BOOST_CHECK_EQUAL(format_time_si(ts("1e-24")),    "+1 ys");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e-23")),   "+10 ys");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e-22")),  "+100 ys");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e-21")),    "+1 zs");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e-20")),   "+10 zs");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e-19")),  "+100 zs");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e-18")),    "+1 as");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e-17")),   "+10 as");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e-16")),  "+100 as");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e-15")),    "+1 fs");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e-14")),   "+10 fs");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e-13")),  "+100 fs");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e-12")),    "+1 ps");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e-11")),   "+10 ps");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e-10")),  "+100 ps");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e-9")),     "+1 ns");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e-8")),    "+10 ns");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e-7")),   "+100 ns");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e-6")),    QString("+1 ") + mu + "s");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e-5")),   QString("+10 ") + mu + "s");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e-4")),  QString("+100 ") + mu + "s");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e-3")),     "+1 ms");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e-2")),    "+10 ms");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e-1")),   "+100 ms");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e0")),       "+1 s");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e1")),      "+10 s");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e2")),     "+100 s");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e3")),      "+1 ks");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e4")),     "+10 ks");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e5")),    "+100 ks");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e6")),      "+1 Ms");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e7")),     "+10 Ms");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e8")),    "+100 Ms");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e9")),      "+1 Gs");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e10")),    "+10 Gs");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e11")),   "+100 Gs");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e12")),     "+1 Ts");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e13")),    "+10 Ts");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e14")),   "+100 Ts");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e15")),     "+1 Ps");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e16")),    "+10 Ps");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e17")),   "+100 Ps");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e18")),     "+1 Es");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e19")),    "+10 Es");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e20")),   "+100 Es");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e21")),     "+1 Zs");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e22")),    "+10 Zs");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e23")),   "+100 Zs");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e24")),     "+1 Ys");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e25")),    "+10 Ys");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e26")),   "+100 Ys");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e27")),  "+1000 Ys");

	BOOST_CHECK_EQUAL(format_time_si(ts("1234")),              "+1 ks");
	BOOST_CHECK_EQUAL(format_time_si(ts("1234"), kilo, 3), "+1.234 ks");
	BOOST_CHECK_EQUAL(format_time_si(ts("1234.5678")),         "+1 ks");

	// check prefix

	BOOST_CHECK_EQUAL(format_time_si(ts("1e-24"), yocto),    "+1 ys");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e-21"), yocto), "+1000 ys");
	BOOST_CHECK_EQUAL(format_time_si(ts("0"), yocto),         "0 ys");

	BOOST_CHECK_EQUAL(format_time_si(ts("1e-4"), milli),         "+0 ms");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e-4"), milli, 1),     "+0.1 ms");
	BOOST_CHECK_EQUAL(format_time_si(ts("1000"), milli),    "+1000000 ms");
	BOOST_CHECK_EQUAL(format_time_si(ts("0"), milli),              "0 ms");

	BOOST_CHECK_EQUAL(format_time_si(ts("1e-1"), none),       "+0 s");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e-1"), none, 1),  "+0.1 s");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e-1"), none, 2), "+0.10 s");
	BOOST_CHECK_EQUAL(format_time_si(ts("1"), none),          "+1 s");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e1"), none),       "+10 s");

	BOOST_CHECK_EQUAL(format_time_si(ts("1e23"), yotta),       "+0 Ys");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e23"), yotta, 1),  "+0.1 Ys");
	BOOST_CHECK_EQUAL(format_time_si(ts("1e27"), yotta),    "+1000 Ys");
	BOOST_CHECK_EQUAL(format_time_si(ts("0"), yotta),           "0 Ys");

	// check precision, rounding

	BOOST_CHECK_EQUAL(format_time_si(ts("1.2345678")),                         "+1 s");
	BOOST_CHECK_EQUAL(format_time_si(ts("1.4")),                               "+1 s");
	BOOST_CHECK_EQUAL(format_time_si(ts("1.5")),                               "+2 s");
	BOOST_CHECK_EQUAL(format_time_si(ts("1.9")),                               "+2 s");
	BOOST_CHECK_EQUAL(format_time_si(ts("1.2345678"), unspecified, 2),      "+1.23 s");
	BOOST_CHECK_EQUAL(format_time_si(ts("1.2345678"), unspecified, 3),     "+1.235 s");
	BOOST_CHECK_EQUAL(format_time_si(ts("1.2345678"), milli, 3),       "+1234.568 ms");
	BOOST_CHECK_EQUAL(format_time_si(ts("1.2345678"), milli, 0),           "+1235 ms");
	BOOST_CHECK_EQUAL(format_time_si(ts("1.2"), unspecified, 3),           "+1.200 s");

	// check unit and sign

	BOOST_CHECK_EQUAL(format_time_si(ts("-1"), none, 0, "V", true),  "-1 V");
	BOOST_CHECK_EQUAL(format_time_si(ts("-1"), none, 0, "V", false), "-1 V");
	BOOST_CHECK_EQUAL(format_time_si(ts("1"), none, 0, "V", true),   "+1 V");
	BOOST_CHECK_EQUAL(format_time_si(ts("1"), none, 0, "V", false),   "1 V");
}

BOOST_AUTO_TEST_CASE(format_time_si_adjusted_test)
{
	BOOST_CHECK_EQUAL(format_time_si_adjusted(ts("-1.5"), milli), "-1500 ms");
	BOOST_CHECK_EQUAL(format_time_si_adjusted(ts("-1.0"), milli), "-1000 ms");
	BOOST_CHECK_EQUAL(format_time_si_adjusted(ts("-0.2"), milli),  "-200 ms");
	BOOST_CHECK_EQUAL(format_time_si_adjusted(ts("-0.1"), milli),  "-100 ms");
	BOOST_CHECK_EQUAL(format_time_si_adjusted(ts( "0.0"), milli),     "0 ms");
	BOOST_CHECK_EQUAL(format_time_si_adjusted(ts( "0.1"), milli),  "+100 ms");
	BOOST_CHECK_EQUAL(format_time_si_adjusted(ts( "0.2"), milli),  "+200 ms");
	BOOST_CHECK_EQUAL(format_time_si_adjusted(ts( "0.3"), milli),  "+300 ms");
	BOOST_CHECK_EQUAL(format_time_si_adjusted(ts( "0.4"), milli),  "+400 ms");
	BOOST_CHECK_EQUAL(format_time_si_adjusted(ts( "0.5"), milli),  "+500 ms");
	BOOST_CHECK_EQUAL(format_time_si_adjusted(ts( "0.6"), milli),  "+600 ms");
	BOOST_CHECK_EQUAL(format_time_si_adjusted(ts( "0.7"), milli),  "+700 ms");
	BOOST_CHECK_EQUAL(format_time_si_adjusted(ts( "0.8"), milli),  "+800 ms");
	BOOST_CHECK_EQUAL(format_time_si_adjusted(ts( "0.9"), milli),  "+900 ms");
	BOOST_CHECK_EQUAL(format_time_si_adjusted(ts( "1.0"), milli), "+1000 ms");
	BOOST_CHECK_EQUAL(format_time_si_adjusted(ts( "1.1"), milli), "+1100 ms");
	BOOST_CHECK_EQUAL(format_time_si_adjusted(ts( "1.2"), milli), "+1200 ms");
	BOOST_CHECK_EQUAL(format_time_si_adjusted(ts( "1.3"), milli), "+1300 ms");
	BOOST_CHECK_EQUAL(format_time_si_adjusted(ts( "1.4"), milli), "+1400 ms");
	BOOST_CHECK_EQUAL(format_time_si_adjusted(ts( "1.5"), milli), "+1500 ms");

	BOOST_CHECK_EQUAL(format_time_si_adjusted(ts( "1.5"), milli, 6), "+1500.000 ms");
	BOOST_CHECK_EQUAL(format_time_si_adjusted(ts( "1.5"), nano,  6), "+1500000000 ns");
	BOOST_CHECK_EQUAL(format_time_si_adjusted(ts( "1.5"), nano,  8), "+1500000000 ns");
	BOOST_CHECK_EQUAL(format_time_si_adjusted(ts( "1.5"), nano,  9), "+1500000000 ns");
	BOOST_CHECK_EQUAL(format_time_si_adjusted(ts( "1.5"), nano, 10), "+1500000000.0 ns");
}

BOOST_AUTO_TEST_CASE(format_time_minutes_test)
{
	using namespace std::placeholders;

	auto fmt = bind(format_time_minutes, _1, _2, true);

	BOOST_CHECK_EQUAL(fmt(ts(    0), 0),    "+0:00");
	BOOST_CHECK_EQUAL(fmt(ts(    1), 0),    "+0:01");
	BOOST_CHECK_EQUAL(fmt(ts(   59), 0),    "+0:59");
	BOOST_CHECK_EQUAL(fmt(ts(   60), 0),    "+1:00");
	BOOST_CHECK_EQUAL(fmt(ts(   -1), 0),    "-0:01");
	BOOST_CHECK_EQUAL(fmt(ts(  -59), 0),    "-0:59");
	BOOST_CHECK_EQUAL(fmt(ts(  -60), 0),    "-1:00");
	BOOST_CHECK_EQUAL(fmt(ts(  100), 0),    "+1:40");
	BOOST_CHECK_EQUAL(fmt(ts( -100), 0),    "-1:40");
	BOOST_CHECK_EQUAL(fmt(ts( 4000), 0), "+1:06:40");
	BOOST_CHECK_EQUAL(fmt(ts(-4000), 0), "-1:06:40");
	BOOST_CHECK_EQUAL(fmt(ts(12000), 0), "+3:20:00");
	BOOST_CHECK_EQUAL(fmt(ts(15000), 0), "+4:10:00");
	BOOST_CHECK_EQUAL(fmt(ts(20000), 0), "+5:33:20");
	BOOST_CHECK_EQUAL(fmt(ts(25000), 0), "+6:56:40");

	BOOST_CHECK_EQUAL(fmt(ts("10641906.007008009"), 0), "+123:04:05:06");
	BOOST_CHECK_EQUAL(fmt(ts("10641906.007008009"), 1), "+123:04:05:06.0");
	BOOST_CHECK_EQUAL(fmt(ts("10641906.007008009"), 2), "+123:04:05:06.01");
	BOOST_CHECK_EQUAL(fmt(ts("10641906.007008009"), 3), "+123:04:05:06.007");
	BOOST_CHECK_EQUAL(fmt(ts("10641906.007008009"), 4), "+123:04:05:06.007 0");
	BOOST_CHECK_EQUAL(fmt(ts("10641906.007008009"), 5), "+123:04:05:06.007 01");
	BOOST_CHECK_EQUAL(fmt(ts("10641906.007008009"), 6), "+123:04:05:06.007 008");
	BOOST_CHECK_EQUAL(fmt(ts("10641906.007008009"), 7), "+123:04:05:06.007 008 0");
	BOOST_CHECK_EQUAL(fmt(ts("10641906.007008009"), 8), "+123:04:05:06.007 008 01");
	BOOST_CHECK_EQUAL(fmt(ts("10641906.007008009"), 9), "+123:04:05:06.007 008 009");

	BOOST_CHECK_EQUAL(format_time_minutes(ts(   0), 0, false),  "0:00");
	BOOST_CHECK_EQUAL(format_time_minutes(ts( 100), 0, false),  "1:40");
	BOOST_CHECK_EQUAL(format_time_minutes(ts(-100), 0, false), "-1:40");
}

BOOST_AUTO_TEST_SUITE_END()
