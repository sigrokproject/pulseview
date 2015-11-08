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

#ifndef PULSEVIEW_PV_DATA_LOGICSEGMENT_HPP
#define PULSEVIEW_PV_DATA_LOGICSEGMENT_HPP

#include "segment.hpp"

#include <utility>
#include <vector>

namespace sigrok {
	class Logic;
}

namespace LogicSegmentTest {
struct Pow2;
struct Basic;
struct LargeData;
struct Pulses;
struct LongPulses;
}

namespace pv {
namespace data {

class LogicSegment : public Segment
{
private:
	struct MipMapLevel
	{
		uint64_t length;
		uint64_t data_length;
		void *data;
	};

private:
	static const unsigned int ScaleStepCount = 10;
	static const int MipMapScalePower;
	static const int MipMapScaleFactor;
	static const float LogMipMapScaleFactor;
	static const uint64_t MipMapDataUnit;

public:
	typedef std::pair<int64_t, bool> EdgePair;

public:
	LogicSegment(std::shared_ptr<sigrok::Logic> logic,
		uint64_t samplerate, uint64_t expected_num_samples = 0);

	virtual ~LogicSegment();

	void append_payload(std::shared_ptr<sigrok::Logic> logic);

	void get_samples(uint8_t *const data,
		int64_t start_sample, int64_t end_sample) const;

private:
	uint64_t unpack_sample(const uint8_t *ptr) const;
	void pack_sample(uint8_t *ptr, uint64_t value);
	
	void reallocate_mipmap_level(MipMapLevel &m);

	void append_payload_to_mipmap();

	uint64_t get_sample(uint64_t index) const;

public:
	/**
	 * Parses a logic data segment to generate a list of transitions
	 * in a time interval to a given level of detail.
	 * @param[out] edges The vector to place the edges into.
	 * @param[in] start The start sample index.
	 * @param[in] end The end sample index.
	 * @param[in] min_length The minimum number of samples that
	 * can be resolved at this level of detail.
	 * @param[in] sig_index The index of the signal.
	 */
	void get_subsampled_edges(std::vector<EdgePair> &edges,
		uint64_t start, uint64_t end,
		float min_length, int sig_index);

private:
	uint64_t get_subsample(int level, uint64_t offset) const;

	static uint64_t pow2_ceil(uint64_t x, unsigned int power);

private:
	struct MipMapLevel mip_map_[ScaleStepCount];
	uint64_t last_append_sample_;

	friend struct LogicSegmentTest::Pow2;
	friend struct LogicSegmentTest::Basic;
	friend struct LogicSegmentTest::LargeData;
	friend struct LogicSegmentTest::Pulses;
	friend struct LogicSegmentTest::LongPulses;
};

} // namespace data
} // namespace pv

#endif // PULSEVIEW_PV_DATA_LOGICSEGMENT_HPP
