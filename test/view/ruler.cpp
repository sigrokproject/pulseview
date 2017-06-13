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

#include <boost/test/floating_point_comparison.hpp>
#include <boost/test/unit_test.hpp>

#include "pv/views/trace/ruler.hpp"
#include "test/test.hpp"

using namespace pv::views::trace;

namespace {
	QString format(const pv::util::Timestamp& t)
	{
		return pv::util::format_time_si(t, pv::util::SIPrefix::none, 6);
	}

	const double e = 0.0001;
};

BOOST_AUTO_TEST_SUITE(RulerTest)

BOOST_AUTO_TEST_CASE(tick_position_test_0)
{
	const pv::util::Timestamp major_period("0.1");
	const pv::util::Timestamp offset("0");
	const double scale(0.001);
	const int width(500);
	const unsigned int minor_tick_count(4);

	const TickPositions ts = Ruler::calculate_tick_positions(
		major_period, offset, scale, width, minor_tick_count, format);

	BOOST_REQUIRE_EQUAL(ts.major.size(), 6);

	BOOST_CHECK_CLOSE(ts.major[0].first,   0, e);
	BOOST_CHECK_CLOSE(ts.major[1].first, 100, e);
	BOOST_CHECK_CLOSE(ts.major[2].first, 200, e);
	BOOST_CHECK_CLOSE(ts.major[3].first, 300, e);
	BOOST_CHECK_CLOSE(ts.major[4].first, 400, e);
	BOOST_CHECK_CLOSE(ts.major[5].first, 500, e);

	BOOST_CHECK_EQUAL(ts.major[0].second,  "0.000000 s");
	BOOST_CHECK_EQUAL(ts.major[1].second, "+0.100000 s");
	BOOST_CHECK_EQUAL(ts.major[2].second, "+0.200000 s");
	BOOST_CHECK_EQUAL(ts.major[3].second, "+0.300000 s");
	BOOST_CHECK_EQUAL(ts.major[4].second, "+0.400000 s");
	BOOST_CHECK_EQUAL(ts.major[5].second, "+0.500000 s");

	BOOST_REQUIRE_EQUAL(ts.minor.size(), 16);

	BOOST_CHECK_CLOSE(ts.minor[ 0], -25, e);
	BOOST_CHECK_CLOSE(ts.minor[ 1],  25, e);
	BOOST_CHECK_CLOSE(ts.minor[ 2],  50, e);
	BOOST_CHECK_CLOSE(ts.minor[ 3],  75, e);
	BOOST_CHECK_CLOSE(ts.minor[ 4], 125, e);
	BOOST_CHECK_CLOSE(ts.minor[ 5], 150, e);
	BOOST_CHECK_CLOSE(ts.minor[ 6], 175, e);
	BOOST_CHECK_CLOSE(ts.minor[ 7], 225, e);
	BOOST_CHECK_CLOSE(ts.minor[ 8], 250, e);
	BOOST_CHECK_CLOSE(ts.minor[ 9], 275, e);
	BOOST_CHECK_CLOSE(ts.minor[10], 325, e);
	BOOST_CHECK_CLOSE(ts.minor[11], 350, e);
	BOOST_CHECK_CLOSE(ts.minor[12], 375, e);
	BOOST_CHECK_CLOSE(ts.minor[13], 425, e);
	BOOST_CHECK_CLOSE(ts.minor[14], 450, e);
	BOOST_CHECK_CLOSE(ts.minor[15], 475, e);
}

BOOST_AUTO_TEST_CASE(tick_position_test_1)
{
	const pv::util::Timestamp major_period("0.1");
	const pv::util::Timestamp offset("-0.463");
	const double scale(0.001);
	const int width(500);
	const unsigned int minor_tick_count(4);

	const TickPositions ts = Ruler::calculate_tick_positions(
		major_period, offset, scale, width, minor_tick_count, format);

	BOOST_REQUIRE_EQUAL(ts.major.size(), 5);

	BOOST_CHECK_CLOSE(ts.major[0].first,   63, e);
	BOOST_CHECK_CLOSE(ts.major[1].first,  163, e);
	BOOST_CHECK_CLOSE(ts.major[2].first,  263, e);
	BOOST_CHECK_CLOSE(ts.major[3].first,  363, e);
	BOOST_CHECK_CLOSE(ts.major[4].first,  463, e);

	BOOST_CHECK_EQUAL(ts.major[0].second, "-0.400000 s");
	BOOST_CHECK_EQUAL(ts.major[1].second, "-0.300000 s");
	BOOST_CHECK_EQUAL(ts.major[2].second, "-0.200000 s");
	BOOST_CHECK_EQUAL(ts.major[3].second, "-0.100000 s");
	BOOST_CHECK_EQUAL(ts.major[4].second,  "0.000000 s");

	BOOST_REQUIRE_EQUAL(ts.minor.size(), 17);
	BOOST_CHECK_CLOSE(ts.minor[ 0], -12, e);
	BOOST_CHECK_CLOSE(ts.minor[ 1],  13, e);
	BOOST_CHECK_CLOSE(ts.minor[ 2],  38, e);
	BOOST_CHECK_CLOSE(ts.minor[ 3],  88, e);
	BOOST_CHECK_CLOSE(ts.minor[ 4], 113, e);
	BOOST_CHECK_CLOSE(ts.minor[ 5], 138, e);
	BOOST_CHECK_CLOSE(ts.minor[ 6], 188, e);
	BOOST_CHECK_CLOSE(ts.minor[ 7], 213, e);
	BOOST_CHECK_CLOSE(ts.minor[ 8], 238, e);
	BOOST_CHECK_CLOSE(ts.minor[ 9], 288, e);
	BOOST_CHECK_CLOSE(ts.minor[10], 313, e);
	BOOST_CHECK_CLOSE(ts.minor[11], 338, e);
	BOOST_CHECK_CLOSE(ts.minor[12], 388, e);
	BOOST_CHECK_CLOSE(ts.minor[13], 413, e);
	BOOST_CHECK_CLOSE(ts.minor[14], 438, e);
	BOOST_CHECK_CLOSE(ts.minor[15], 488, e);
	BOOST_CHECK_CLOSE(ts.minor[16], 513, e);
}

BOOST_AUTO_TEST_CASE(tick_position_test_2)
{
	const pv::util::Timestamp major_period("20");
	const pv::util::Timestamp offset("8");
	const double scale(0.129746);
	const int width(580);
	const unsigned int minor_tick_count(4);

	const TickPositions ts = Ruler::calculate_tick_positions(
		major_period, offset, scale, width, minor_tick_count, format);

	const double mp = 5;
	const int off = 8;

	BOOST_REQUIRE_EQUAL(ts.major.size(), 4);

	BOOST_CHECK_CLOSE(ts.major[0].first, ( 4 * mp - off) / scale, e);
	BOOST_CHECK_CLOSE(ts.major[1].first, ( 8 * mp - off) / scale, e);
	BOOST_CHECK_CLOSE(ts.major[2].first, (12 * mp - off) / scale, e);
	BOOST_CHECK_CLOSE(ts.major[3].first, (16 * mp - off) / scale, e);

	BOOST_CHECK_EQUAL(ts.major[0].second, "+20.000000 s");
	BOOST_CHECK_EQUAL(ts.major[1].second, "+40.000000 s");
	BOOST_CHECK_EQUAL(ts.major[2].second, "+60.000000 s");
	BOOST_CHECK_EQUAL(ts.major[3].second, "+80.000000 s");

	BOOST_REQUIRE_EQUAL(ts.minor.size(), 13);

	BOOST_CHECK_CLOSE(ts.minor[ 0], ( 1 * mp - off) / scale, e);
	BOOST_CHECK_CLOSE(ts.minor[ 1], ( 2 * mp - off) / scale, e);
	BOOST_CHECK_CLOSE(ts.minor[ 2], ( 3 * mp - off) / scale, e);
	BOOST_CHECK_CLOSE(ts.minor[ 3], ( 5 * mp - off) / scale, e);
	BOOST_CHECK_CLOSE(ts.minor[ 4], ( 6 * mp - off) / scale, e);
	BOOST_CHECK_CLOSE(ts.minor[ 5], ( 7 * mp - off) / scale, e);
	BOOST_CHECK_CLOSE(ts.minor[ 6], ( 9 * mp - off) / scale, e);
	BOOST_CHECK_CLOSE(ts.minor[ 7], (10 * mp - off) / scale, e);
	BOOST_CHECK_CLOSE(ts.minor[ 8], (11 * mp - off) / scale, e);
	BOOST_CHECK_CLOSE(ts.minor[ 9], (13 * mp - off) / scale, e);
	BOOST_CHECK_CLOSE(ts.minor[10], (14 * mp - off) / scale, e);
	BOOST_CHECK_CLOSE(ts.minor[11], (15 * mp - off) / scale, e);
	BOOST_CHECK_CLOSE(ts.minor[12], (17 * mp - off) / scale, e);
}

BOOST_AUTO_TEST_SUITE_END()
