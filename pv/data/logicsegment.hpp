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

#ifndef PULSEVIEW_PV_DATA_LOGICSEGMENT_HPP
#define PULSEVIEW_PV_DATA_LOGICSEGMENT_HPP

#include "segment.hpp"

#include <vector>

#include <QObject>

using std::enable_shared_from_this;
using std::pair;
using std::shared_ptr;
using std::vector;

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

class Logic;

class LogicSegment : public Segment, public enable_shared_from_this<Segment>
{
	Q_OBJECT

public:
	typedef pair<int64_t, bool> EdgePair;

	static const unsigned int ScaleStepCount = 10;
	static const int MipMapScalePower;
	static const int MipMapScaleFactor;
	static const float LogMipMapScaleFactor;
	static const uint64_t MipMapDataUnit;

private:
	struct MipMapLevel
	{
		uint64_t length;
		uint64_t data_length;
		void *data;
	};

public:
	LogicSegment(pv::data::Logic& owner, uint32_t segment_id,
		unsigned int unit_size, uint64_t samplerate);

	virtual ~LogicSegment();

	/**
	 * Using enable_shared_from_this prevents the normal use of shared_ptr
	 * instances by users of LogicSegment instances. Instead, shared_ptrs may
	 * only be created by the instance itself.
	 * See https://en.cppreference.com/w/cpp/memory/enable_shared_from_this
	 */
	shared_ptr<const LogicSegment> get_shared_ptr() const;

	void append_payload(shared_ptr<sigrok::Logic> logic);
	void append_payload(void *data, uint64_t data_size);

	/**
	 * Appends sample data for a single channel where each byte
	 * represents one sample - if it's 0 the state is low, if 1 high.
	 * Other values are not permitted.
	 * Assumes that all channels are having samples added and in the
	 * order of 0..n, not n..0.
	 * Also assumes the the number of samples added for each channel
	 * is constant for every invokation for 0..n. The number of samples
	 * hence may only change when index is 0.
	 */
	void append_subsignal_payload(unsigned int index, void *data,
		uint64_t data_size, vector<uint8_t>& destination);

	void get_samples(int64_t start_sample, int64_t end_sample, uint8_t* dest) const;

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
	void get_subsampled_edges(vector<EdgePair> &edges,
		uint64_t start, uint64_t end,
		float min_length, int sig_index, bool first_change_only = false);

	void get_surrounding_edges(vector<EdgePair> &dest,
		uint64_t origin_sample, float min_length, int sig_index);

private:
	uint64_t unpack_sample(const uint8_t *ptr) const;
	void pack_sample(uint8_t *ptr, uint64_t value);

	void reallocate_mipmap_level(MipMapLevel &m);

	void append_payload_to_mipmap();

	uint64_t get_unpacked_sample(uint64_t index) const;

	template <class T> void downsampleTmain(const T*&in, T &acc, T &prev);
	template <class T> void downsampleT(const uint8_t *in, uint8_t *&out, uint64_t len);
	void downsampleGeneric(const uint8_t *in, uint8_t *&out, uint64_t len);

private:
	uint64_t get_subsample(int level, uint64_t offset) const;

	static uint64_t pow2_ceil(uint64_t x, unsigned int power);

private:
	Logic& owner_;

	struct MipMapLevel mip_map_[ScaleStepCount];
	uint64_t last_append_sample_;
	uint64_t last_append_accumulator_;
	uint64_t last_append_extra_;

	friend struct LogicSegmentTest::Pow2;
	friend struct LogicSegmentTest::Basic;
	friend struct LogicSegmentTest::LargeData;
	friend struct LogicSegmentTest::Pulses;
	friend struct LogicSegmentTest::LongPulses;
};

} // namespace data
} // namespace pv

#endif // PULSEVIEW_PV_DATA_LOGICSEGMENT_HPP
