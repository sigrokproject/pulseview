/*
 * This file is part of the sigrok project.
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

#define __STDC_LIMIT_MACROS
#include <stdint.h>

#include <boost/test/unit_test.hpp>

#include "../logicdatasnapshot.h"

using namespace std;

BOOST_AUTO_TEST_SUITE(LogicDataSnapshotTest)

void push_logic(LogicDataSnapshot &s, unsigned int length, uint8_t value)
{
	sr_datafeed_logic logic;
	logic.unitsize = 1;
	logic.length = length;
	logic.data = new uint8_t[length];
	memset(logic.data, value, length * logic.unitsize);
	s.append_payload(logic);
	delete[] (uint8_t*)logic.data;
}

BOOST_AUTO_TEST_CASE(Pow2)
{
	BOOST_CHECK_EQUAL(LogicDataSnapshot::pow2_ceil(-2, 0), -2);
	BOOST_CHECK_EQUAL(LogicDataSnapshot::pow2_ceil(-1, 0), -1);
	BOOST_CHECK_EQUAL(LogicDataSnapshot::pow2_ceil(0, 0), 0);
	BOOST_CHECK_EQUAL(LogicDataSnapshot::pow2_ceil(1, 0), 1);
	BOOST_CHECK_EQUAL(LogicDataSnapshot::pow2_ceil(2, 0), 2);

	BOOST_CHECK_EQUAL(
		LogicDataSnapshot::pow2_ceil(INT64_MIN, 0), INT64_MIN);
	BOOST_CHECK_EQUAL(
		LogicDataSnapshot::pow2_ceil(INT64_MAX, 0), INT64_MAX);

	BOOST_CHECK_EQUAL(LogicDataSnapshot::pow2_ceil(-3, 1), -2);
	BOOST_CHECK_EQUAL(LogicDataSnapshot::pow2_ceil(-2, 1), -2);
	BOOST_CHECK_EQUAL(LogicDataSnapshot::pow2_ceil(-1, 1), 0);
	BOOST_CHECK_EQUAL(LogicDataSnapshot::pow2_ceil(0, 1), 0);
	BOOST_CHECK_EQUAL(LogicDataSnapshot::pow2_ceil(1, 1), 2);
	BOOST_CHECK_EQUAL(LogicDataSnapshot::pow2_ceil(2, 1), 2);
	BOOST_CHECK_EQUAL(LogicDataSnapshot::pow2_ceil(3, 1), 4);
}

BOOST_AUTO_TEST_CASE(Basic)
{
	// Create an empty LogicDataSnapshot object
	sr_datafeed_logic logic;
	logic.length = 0;
	logic.unitsize = 1;
	logic.data = NULL;

	LogicDataSnapshot s(logic);

	//----- Test LogicDataSnapshot::push_logic -----//

	BOOST_CHECK(s.get_sample_count() == 0);
	for(int i = 0; i < LogicDataSnapshot::ScaleStepCount; i++)
	{
		const LogicDataSnapshot::MipMapLevel &m = s._mip_map[i];
		BOOST_CHECK_EQUAL(m.length, 0);
		BOOST_CHECK_EQUAL(m.data_length, 0);
		BOOST_CHECK(m.data == NULL);
	}

	// Push 8 samples of all zeros
	push_logic(s, 8, 0);

	BOOST_CHECK(s.get_sample_count() == 8);

	// There should not be enough samples to have a single mip map sample
	for(int i = 0; i < LogicDataSnapshot::ScaleStepCount; i++)
	{
		const LogicDataSnapshot::MipMapLevel &m = s._mip_map[i];
		BOOST_CHECK_EQUAL(m.length, 0);
		BOOST_CHECK_EQUAL(m.data_length, 0);
		BOOST_CHECK(m.data == NULL);
	}

	// Push 8 samples of 0x11s to bring the total up to 16
	push_logic(s, 8, 0x11);

	// There should now be enough data for exactly one sample
	// in mip map level 0, and that sample should be 0
	const LogicDataSnapshot::MipMapLevel &m0 = s._mip_map[0];
	BOOST_CHECK_EQUAL(m0.length, 1);
	BOOST_CHECK_EQUAL(m0.data_length, LogicDataSnapshot::MipMapDataUnit);
	BOOST_REQUIRE(m0.data != NULL);
	BOOST_CHECK_EQUAL(((uint8_t*)m0.data)[0], 0x11);

	// The higher levels should still be empty
	for(int i = 1; i < LogicDataSnapshot::ScaleStepCount; i++)
	{
		const LogicDataSnapshot::MipMapLevel &m = s._mip_map[i];
		BOOST_CHECK_EQUAL(m.length, 0);
		BOOST_CHECK_EQUAL(m.data_length, 0);
		BOOST_CHECK(m.data == NULL);
	}

	// Push 240 samples of all zeros to bring the total up to 256
	push_logic(s, 240, 0);

	BOOST_CHECK_EQUAL(m0.length, 16);
	BOOST_CHECK_EQUAL(m0.data_length, LogicDataSnapshot::MipMapDataUnit);

	BOOST_CHECK_EQUAL(((uint8_t*)m0.data)[1], 0x11);
	for(int i = 2; i < m0.length; i++)
		BOOST_CHECK_EQUAL(((uint8_t*)m0.data)[i], 0);

	const LogicDataSnapshot::MipMapLevel &m1 = s._mip_map[1];
	BOOST_CHECK_EQUAL(m1.length, 1);
	BOOST_CHECK_EQUAL(m1.data_length, LogicDataSnapshot::MipMapDataUnit);
	BOOST_REQUIRE(m1.data != NULL);
	BOOST_CHECK_EQUAL(((uint8_t*)m1.data)[0], 0x11);

	//----- Test LogicDataSnapshot::get_subsampled_edges -----//

	// Test a full view at full zoom.
	vector<LogicDataSnapshot::EdgePair> edges;
	s.get_subsampled_edges(edges, 0, 255, 1, 0);
	BOOST_REQUIRE_EQUAL(edges.size(), 4);

	BOOST_CHECK_EQUAL(edges[0].first, 0);
	BOOST_CHECK_EQUAL(edges[1].first, 8);
	BOOST_CHECK_EQUAL(edges[2].first, 16);
	BOOST_CHECK_EQUAL(edges[3].first, 255);

	// Test a subset at high zoom
	edges.clear();
	s.get_subsampled_edges(edges, 6, 17, 0.05f, 0);
	BOOST_REQUIRE_EQUAL(edges.size(), 4);

	BOOST_CHECK_EQUAL(edges[0].first, 6);
	BOOST_CHECK_EQUAL(edges[1].first, 8);
	BOOST_CHECK_EQUAL(edges[2].first, 16);
	BOOST_CHECK_EQUAL(edges[3].first, 17);
}

BOOST_AUTO_TEST_CASE(LargeData)
{
	uint8_t prev_sample;
	const int Length = 1000000;

	sr_datafeed_logic logic;
	logic.unitsize = 1;
	logic.length = Length;
	logic.data = new uint8_t[Length];
	uint8_t *data = (uint8_t*)logic.data;

	for(int i = 0; i < Length; i++)
		*data++ = (uint8_t)(i >> 8);

	LogicDataSnapshot s(logic);
	delete[] (uint8_t*)logic.data;

	BOOST_CHECK(s.get_sample_count() == Length);

	// Check mip map level 0
	BOOST_CHECK_EQUAL(s._mip_map[0].length, 62500);
	BOOST_CHECK_EQUAL(s._mip_map[0].data_length,
		LogicDataSnapshot::MipMapDataUnit);
	BOOST_REQUIRE(s._mip_map[0].data != NULL);

	prev_sample = 0;
	for(int i = 0; i < s._mip_map[0].length;)
	{
		BOOST_TEST_MESSAGE("Testing mip_map[0].data[" << i << "]");

		const uint8_t sample = (uint8_t)((i*16) >> 8);
		BOOST_CHECK_EQUAL(s.get_subsample(0, i++) & 0xFF,
			prev_sample ^ sample);
		prev_sample = sample;

		for(int j = 1; i < s._mip_map[0].length && j < 16; j++)
		{
			BOOST_TEST_MESSAGE("Testing mip_map[0].data[" << i << "]");
			BOOST_CHECK_EQUAL(s.get_subsample(0, i++) & 0xFF, 0);
		}
	}

	// Check mip map level 1
	BOOST_CHECK_EQUAL(s._mip_map[1].length, 3906);
	BOOST_CHECK_EQUAL(s._mip_map[1].data_length,
		LogicDataSnapshot::MipMapDataUnit);
	BOOST_REQUIRE(s._mip_map[1].data != NULL);

	prev_sample = 0;
	for(int i = 0; i < s._mip_map[1].length; i++)
	{
		BOOST_TEST_MESSAGE("Testing mip_map[1].data[" << i << "]");

		const uint8_t sample = i;
		const uint8_t expected = sample ^ prev_sample;
		prev_sample = i;

		BOOST_CHECK_EQUAL(s.get_subsample(1, i) & 0xFF, expected);
	}

	// Check mip map level 2
	BOOST_CHECK_EQUAL(s._mip_map[2].length, 244);
	BOOST_CHECK_EQUAL(s._mip_map[2].data_length,
		LogicDataSnapshot::MipMapDataUnit);
	BOOST_REQUIRE(s._mip_map[2].data != NULL);

	prev_sample = 0;
	for(int i = 0; i < s._mip_map[2].length; i++)
	{
		BOOST_TEST_MESSAGE("Testing mip_map[2].data[" << i << "]");

		const uint8_t sample = i << 4;
		const uint8_t expected = (sample ^ prev_sample) | 0x0F;
		prev_sample = sample;

		BOOST_CHECK_EQUAL(s.get_subsample(2, i) & 0xFF, expected);
	}

	// Check mip map level 3
	BOOST_CHECK_EQUAL(s._mip_map[3].length, 15);
	BOOST_CHECK_EQUAL(s._mip_map[3].data_length,
		LogicDataSnapshot::MipMapDataUnit);
	BOOST_REQUIRE(s._mip_map[3].data != NULL);

	for(int i = 0; i < s._mip_map[3].length; i++)
		BOOST_CHECK_EQUAL(*((uint8_t*)s._mip_map[3].data + i),
			0xFF);

	// Check the higher levels
	for(int i = 4; i < LogicDataSnapshot::ScaleStepCount; i++)
	{
		const LogicDataSnapshot::MipMapLevel &m = s._mip_map[i];
		BOOST_CHECK_EQUAL(m.length, 0);
		BOOST_CHECK_EQUAL(m.data_length, 0);
		BOOST_CHECK(m.data == NULL);
	}

	//----- Test LogicDataSnapshot::get_subsampled_edges -----//
	vector<LogicDataSnapshot::EdgePair> edges;

	s.get_subsampled_edges(edges, 0, Length-1, 1, 7);

	BOOST_CHECK_EQUAL(edges.size(), 32);

	for(int i = 0; i < edges.size() - 1; i++)
	{
		BOOST_CHECK_EQUAL(edges[i].first, i * 32768);
		BOOST_CHECK_EQUAL(edges[i].second, i & 1);
	}

	BOOST_CHECK_EQUAL(edges[31].first, 999999);
}

BOOST_AUTO_TEST_SUITE_END()
