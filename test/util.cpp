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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <boost/test/unit_test.hpp>

#include "pv/util.hpp"

using namespace pv::util;
using ts = pv::util::Timestamp;

namespace {
	QChar mu = QChar(0x03BC);

	pv::util::SIPrefix unspecified = pv::util::SIPrefix::unspecified;
	pv::util::SIPrefix yocto       = pv::util::SIPrefix::yocto;
	pv::util::SIPrefix micro       = pv::util::SIPrefix::micro;
	pv::util::SIPrefix milli       = pv::util::SIPrefix::milli;
	pv::util::SIPrefix none        = pv::util::SIPrefix::none;
	pv::util::SIPrefix kilo        = pv::util::SIPrefix::kilo;
	pv::util::SIPrefix yotta       = pv::util::SIPrefix::yotta;

	pv::util::TimeUnit Time = pv::util::TimeUnit::Time;
}

std::ostream& operator<<(std::ostream& stream, const QString& str)
{
	return stream << str.toUtf8().data();
}

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

BOOST_AUTO_TEST_CASE(format_si_value_test)
{
	// check prefix calculation

	BOOST_CHECK_EQUAL(format_si_value(ts("0"), "V"), "0 V");

	BOOST_CHECK_EQUAL(format_si_value(ts("1e-24"), "V"),   "+1 yV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e-23"), "V"),  "+10 yV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e-22"), "V"), "+100 yV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e-21"), "V"),   "+1 zV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e-20"), "V"),  "+10 zV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e-19"), "V"), "+100 zV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e-18"), "V"),   "+1 aV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e-17"), "V"),  "+10 aV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e-16"), "V"), "+100 aV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e-15"), "V"),   "+1 fV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e-14"), "V"),  "+10 fV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e-13"), "V"), "+100 fV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e-12"), "V"),   "+1 pV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e-11"), "V"),  "+10 pV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e-10"), "V"), "+100 pV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e-9"), "V"),    "+1 nV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e-8"), "V"),   "+10 nV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e-7"), "V"),  "+100 nV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e-6"), "V"),   QString("+1 ") + mu + "V");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e-5"), "V"),  QString("+10 ") + mu + "V");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e-4"), "V"), QString("+100 ") + mu + "V");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e-3"), "V"),    "+1 mV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e-2"), "V"),   "+10 mV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e-1"), "V"),  "+100 mV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e0"), "V"),      "+1 V");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e1"), "V"),     "+10 V");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e2"), "V"),    "+100 V");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e3"), "V"),     "+1 kV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e4"), "V"),    "+10 kV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e5"), "V"),   "+100 kV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e6"), "V"),     "+1 MV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e7"), "V"),    "+10 MV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e8"), "V"),   "+100 MV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e9"), "V"),     "+1 GV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e10"), "V"),   "+10 GV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e11"), "V"),  "+100 GV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e12"), "V"),    "+1 TV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e13"), "V"),   "+10 TV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e14"), "V"),  "+100 TV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e15"), "V"),    "+1 PV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e16"), "V"),   "+10 PV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e17"), "V"),  "+100 PV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e18"), "V"),    "+1 EV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e19"), "V"),   "+10 EV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e20"), "V"),  "+100 EV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e21"), "V"),    "+1 ZV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e22"), "V"),   "+10 ZV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e23"), "V"),  "+100 ZV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e24"), "V"),    "+1 YV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e25"), "V"),   "+10 YV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e26"), "V"),  "+100 YV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e27"), "V"), "+1000 YV");

	BOOST_CHECK_EQUAL(format_si_value(ts("1234"), "V"),           "+1 kV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1234"), "V", kilo, 3), "+1.234 kV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1234.5678"), "V"),      "+1 kV");

	// check if a given prefix is honored

	BOOST_CHECK_EQUAL(format_si_value(ts("1e-24"), "V", yocto),    "+1 yV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e-21"), "V", yocto), "+1000 yV");
	BOOST_CHECK_EQUAL(format_si_value(ts("0"), "V", yocto),         "0 yV");

	BOOST_CHECK_EQUAL(format_si_value(ts("1e-4"), "V", milli),       "+0 mV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e-4"), "V", milli, 1),  "+0.1 mV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1000"), "V", milli), "+1000000 mV");
	BOOST_CHECK_EQUAL(format_si_value(ts("0"), "V", milli),           "0 mV");

	BOOST_CHECK_EQUAL(format_si_value(ts("1e-1"), "V", none),       "+0 V");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e-1"), "V", none, 1),  "+0.1 V");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e-1"), "V", none, 2), "+0.10 V");
	BOOST_CHECK_EQUAL(format_si_value(ts("1"), "V", none),          "+1 V");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e1"), "V", none),       "+10 V");

	BOOST_CHECK_EQUAL(format_si_value(ts("1e23"), "V", yotta),       "+0 YV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e23"), "V", yotta, 1),  "+0.1 YV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1e27"), "V", yotta),    "+1000 YV");
	BOOST_CHECK_EQUAL(format_si_value(ts("0"), "V", yotta),           "0 YV");

	// check precision

	BOOST_CHECK_EQUAL(format_si_value(ts("1.2345678"), "V"),                "+1 V");
	BOOST_CHECK_EQUAL(format_si_value(ts("1.4"), "V"),                      "+1 V");
	BOOST_CHECK_EQUAL(format_si_value(ts("1.5"), "V"),                      "+2 V");
	BOOST_CHECK_EQUAL(format_si_value(ts("1.9"), "V"),                      "+2 V");
	BOOST_CHECK_EQUAL(format_si_value(ts("1.2345678"), "V", unspecified, 2),      "+1.23 V");
	BOOST_CHECK_EQUAL(format_si_value(ts("1.2345678"), "V", unspecified, 3),     "+1.235 V");
	BOOST_CHECK_EQUAL(format_si_value(ts("1.2345678"), "V", milli, 3),       "+1234.568 mV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1.2345678"), "V", milli, 0),           "+1235 mV");
	BOOST_CHECK_EQUAL(format_si_value(ts("1.2"), "V", unspecified, 3),           "+1.200 V");

	// check sign

	BOOST_CHECK_EQUAL(format_si_value(ts("-1"), "V", none, 0, true),  "-1 V");
	BOOST_CHECK_EQUAL(format_si_value(ts("-1"), "V", none, 0, false), "-1 V");
	BOOST_CHECK_EQUAL(format_si_value(ts("1"), "V", none, 0, true),   "+1 V");
	BOOST_CHECK_EQUAL(format_si_value(ts("1"), "V", none, 0, false),   "1 V");
}

BOOST_AUTO_TEST_CASE(format_time_test)
{
	BOOST_CHECK_EQUAL(format_time(ts("-0.00005"), micro, Time, 5), QString("-50 ") + mu + "s");
	BOOST_CHECK_EQUAL(format_time(ts( "0.00005"), micro, Time, 5), QString("+50 ") + mu + "s");
	BOOST_CHECK_EQUAL(format_time(ts( "1")), "+1 s");
	BOOST_CHECK_EQUAL(format_time(ts("-1")), "-1 s");
	BOOST_CHECK_EQUAL(format_time(ts( "100")),                    "+1:40");
	BOOST_CHECK_EQUAL(format_time(ts("-100")),                    "-1:40");
	BOOST_CHECK_EQUAL(format_time(ts( "4000")),                "+1:06:40");
	BOOST_CHECK_EQUAL(format_time(ts("-4000")),                "-1:06:40");
	BOOST_CHECK_EQUAL(format_time(ts("12000"), kilo, Time, 0), "+3:20:00");
	BOOST_CHECK_EQUAL(format_time(ts("15000"), kilo, Time, 0), "+4:10:00");
	BOOST_CHECK_EQUAL(format_time(ts("20000"), kilo, Time, 0), "+5:33:20");
	BOOST_CHECK_EQUAL(format_time(ts("25000"), kilo, Time, 0), "+6:56:40");

	BOOST_CHECK_EQUAL(format_time(ts("10641906.007008009"), none, Time, 0), "+123:04:05:06");
	BOOST_CHECK_EQUAL(format_time(ts("10641906.007008009"), none, Time, 1), "+123:04:05:06.0");
	BOOST_CHECK_EQUAL(format_time(ts("10641906.007008009"), none, Time, 2), "+123:04:05:06.00");
	BOOST_CHECK_EQUAL(format_time(ts("10641906.007008009"), none, Time, 3), "+123:04:05:06.007");
	BOOST_CHECK_EQUAL(format_time(ts("10641906.007008009"), none, Time, 4), "+123:04:05:06.007 0");
	BOOST_CHECK_EQUAL(format_time(ts("10641906.007008009"), none, Time, 5), "+123:04:05:06.007 00");
	BOOST_CHECK_EQUAL(format_time(ts("10641906.007008009"), none, Time, 6), "+123:04:05:06.007 008");
	BOOST_CHECK_EQUAL(format_time(ts("10641906.007008009"), none, Time, 7), "+123:04:05:06.007 008 0");
	BOOST_CHECK_EQUAL(format_time(ts("10641906.007008009"), none, Time, 8), "+123:04:05:06.007 008 00");
	BOOST_CHECK_EQUAL(format_time(ts("10641906.007008009"), none, Time, 9), "+123:04:05:06.007 008 009");

	BOOST_CHECK_EQUAL(format_time(ts("-1.5"), milli), "-1500 ms");
	BOOST_CHECK_EQUAL(format_time(ts("-1.0"), milli), "-1000 ms");
	BOOST_CHECK_EQUAL(format_time(ts("-0.2")),         "-200 ms");
	BOOST_CHECK_EQUAL(format_time(ts("-0.1")),         "-100 ms");
	BOOST_CHECK_EQUAL(format_time(ts("0.0")),             "0");
	BOOST_CHECK_EQUAL(format_time(ts("0.1")),          "+100 ms");
	BOOST_CHECK_EQUAL(format_time(ts("0.2")),          "+200 ms");
	BOOST_CHECK_EQUAL(format_time(ts("0.3")),          "+300 ms");
	BOOST_CHECK_EQUAL(format_time(ts("0.4")),          "+400 ms");
	BOOST_CHECK_EQUAL(format_time(ts("0.5")),          "+500 ms");
	BOOST_CHECK_EQUAL(format_time(ts("0.6")),          "+600 ms");
	BOOST_CHECK_EQUAL(format_time(ts("0.7")),          "+700 ms");
	BOOST_CHECK_EQUAL(format_time(ts("0.8")),          "+800 ms");
	BOOST_CHECK_EQUAL(format_time(ts("0.9")),          "+900 ms");
	BOOST_CHECK_EQUAL(format_time(ts("1.0"), milli),  "+1000 ms");
	BOOST_CHECK_EQUAL(format_time(ts("1.1"), milli),  "+1100 ms");
	BOOST_CHECK_EQUAL(format_time(ts("1.2"), milli),  "+1200 ms");
	BOOST_CHECK_EQUAL(format_time(ts("1.3"), milli),  "+1300 ms");
	BOOST_CHECK_EQUAL(format_time(ts("1.4"), milli),  "+1400 ms");
	BOOST_CHECK_EQUAL(format_time(ts("1.5"), milli),  "+1500 ms");
}

BOOST_AUTO_TEST_SUITE_END()
