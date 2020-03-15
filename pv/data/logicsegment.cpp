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

#include "config.h" // For HAVE_UNALIGNED_LITTLE_ENDIAN_ACCESS

#include <extdef.h>

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdint>

#include "logic.hpp"
#include "logicsegment.hpp"

#include <libsigrokcxx/libsigrokcxx.hpp>

using std::lock_guard;
using std::recursive_mutex;
using std::max;
using std::min;
using std::shared_ptr;
using std::vector;

using sigrok::Logic;

namespace pv {
namespace data {

const int LogicSegment::MipMapScalePower = 4;
const int LogicSegment::MipMapScaleFactor = 1 << MipMapScalePower;
const float LogicSegment::LogMipMapScaleFactor = logf(MipMapScaleFactor);
const uint64_t LogicSegment::MipMapDataUnit = 64 * 1024; // bytes

LogicSegment::LogicSegment(pv::data::Logic& owner, uint32_t segment_id,
	unsigned int unit_size,	uint64_t samplerate) :
	Segment(segment_id, samplerate, unit_size),
	owner_(owner),
	last_append_sample_(0),
	last_append_accumulator_(0),
	last_append_extra_(0)
{
	memset(mip_map_, 0, sizeof(mip_map_));
}

LogicSegment::~LogicSegment()
{
	lock_guard<recursive_mutex> lock(mutex_);

	for (MipMapLevel &l : mip_map_)
		free(l.data);
}

shared_ptr<const LogicSegment> LogicSegment::get_shared_ptr() const
{
	shared_ptr<const Segment> ptr = nullptr;

	try {
		ptr = shared_from_this();
	} catch (std::exception& e) {
		/* Do nothing, ptr remains a null pointer */
	}

	return ptr ? std::dynamic_pointer_cast<const LogicSegment>(ptr) : nullptr;
}

template <class T>
void LogicSegment::downsampleTmain(const T*&in, T &acc, T &prev)
{
	// Accumulate one sample at a time
	for (uint64_t i = 0; i < MipMapScaleFactor; i++) {
		T sample = *in++;
		acc |= prev ^ sample;
		prev = sample;
	}
}

template <>
void LogicSegment::downsampleTmain<uint8_t>(const uint8_t*&in, uint8_t &acc, uint8_t &prev)
{
	// Handle 8 bit samples in 32 bit steps
	uint32_t prev32 = prev | prev << 8 | prev << 16 | prev << 24;
	uint32_t acc32 = acc;
	const uint32_t *in32 = (const uint32_t*)in;
	for (uint64_t i = 0; i < MipMapScaleFactor; i += 4) {
		uint32_t sample32 = *in32++;
		acc32 |= prev32 ^ sample32;
		prev32 = sample32;
	}
	// Reduce result back to uint8_t
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	prev = (prev32 >> 24) & 0xff; // MSB is last
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	prev = prev32 & 0xff; // LSB is last
#else
#error Endianness unknown
#endif
	acc |= acc32 & 0xff;
	acc |= (acc32 >> 8) & 0xff;
	acc |= (acc32 >> 16) & 0xff;
	acc |= (acc32 >> 24) & 0xff;
	in = (const uint8_t*)in32;
}

template <>
void LogicSegment::downsampleTmain<uint16_t>(const uint16_t*&in, uint16_t &acc, uint16_t &prev)
{
	// Handle 16 bit samples in 32 bit steps
	uint32_t prev32 = prev | prev << 16;
	uint32_t acc32 = acc;
	const uint32_t *in32 = (const uint32_t*)in;
	for (uint64_t i = 0; i < MipMapScaleFactor; i += 2) {
		uint32_t sample32 = *in32++;
		acc32 |= prev32 ^ sample32;
		prev32 = sample32;
	}
	// Reduce result back to uint16_t
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	prev = (prev32 >> 16) & 0xffff; // MSB is last
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	prev = prev32 & 0xffff; // LSB is last
#else
#error Endian unknown
#endif
	acc |= acc32 & 0xffff;
	acc |= (acc32 >> 16) & 0xffff;
	in = (const uint16_t*)in32;
}

template <class T>
void LogicSegment::downsampleT(const uint8_t *in_, uint8_t *&out_, uint64_t len)
{
	const T *in = (const T*)in_;
	T *out = (T*)out_;
	T prev = last_append_sample_;
	T acc = last_append_accumulator_;

	// Try to complete the previous downsample
	if (last_append_extra_) {
		while (last_append_extra_ < MipMapScaleFactor && len > 0) {
			T sample = *in++;
			acc |= prev ^ sample;
			prev = sample;
			last_append_extra_++;
			len--;
		}
		if (!len) {
			// Not enough samples available to complete downsample
			last_append_sample_ = prev;
			last_append_accumulator_ = acc;
			return;
		}
		// We have a complete downsample
		*out++ = acc;
		acc = 0;
		last_append_extra_ = 0;
	}

	// Handle complete blocks of MipMapScaleFactor samples
	while (len >= MipMapScaleFactor) {
		downsampleTmain<T>(in, acc, prev);
		len -= MipMapScaleFactor;
		// Output downsample
		*out++ = acc;
		acc = 0;
	}

	// Process remainder, not enough for a complete sample
	while (len > 0) {
		T sample = *in++;
		acc |= prev ^ sample;
		prev = sample;
		last_append_extra_++;
		len--;
	}

	// Update context
	last_append_sample_ = prev;
	last_append_accumulator_ = acc;
	out_ = (uint8_t *)out;
}

void LogicSegment::downsampleGeneric(const uint8_t *in, uint8_t *&out, uint64_t len)
{
	// Downsample using the generic unpack_sample()
	// which can handle any width between 1 and 8 bytes
	uint64_t prev = last_append_sample_;
	uint64_t acc = last_append_accumulator_;

	// Try to complete the previous downsample
	if (last_append_extra_) {
		while (last_append_extra_ < MipMapScaleFactor && len > 0) {
			const uint64_t sample = unpack_sample(in);
			in += unit_size_;
			acc |= prev ^ sample;
			prev = sample;
			last_append_extra_++;
			len--;
		}
		if (!len) {
			// Not enough samples available to complete downsample
			last_append_sample_ = prev;
			last_append_accumulator_ = acc;
			return;
		}
		// We have a complete downsample
		pack_sample(out, acc);
		out += unit_size_;
		acc = 0;
		last_append_extra_ = 0;
	}

	// Handle complete blocks of MipMapScaleFactor samples
	while (len >= MipMapScaleFactor) {
		// Accumulate one sample at a time
		for (uint64_t i = 0; i < MipMapScaleFactor; i++) {
			const uint64_t sample = unpack_sample(in);
			in += unit_size_;
			acc |= prev ^ sample;
			prev = sample;
		}
		len -= MipMapScaleFactor;
		// Output downsample
		pack_sample(out, acc);
		out += unit_size_;
		acc = 0;
	}

	// Process remainder, not enough for a complete sample
	while (len > 0) {
		const uint64_t sample = unpack_sample(in);
		in += unit_size_;
		acc |= prev ^ sample;
		prev = sample;
		last_append_extra_++;
		len--;
	}

	// Update context
	last_append_sample_ = prev;
	last_append_accumulator_ = acc;
}

inline uint64_t LogicSegment::unpack_sample(const uint8_t *ptr) const
{
#ifdef HAVE_UNALIGNED_LITTLE_ENDIAN_ACCESS
	return *(uint64_t*)ptr;
#else
	uint64_t value = 0;
	switch (unit_size_) {
	default:
		value |= ((uint64_t)ptr[7]) << 56;
		/* FALLTHRU */
	case 7:
		value |= ((uint64_t)ptr[6]) << 48;
		/* FALLTHRU */
	case 6:
		value |= ((uint64_t)ptr[5]) << 40;
		/* FALLTHRU */
	case 5:
		value |= ((uint64_t)ptr[4]) << 32;
		/* FALLTHRU */
	case 4:
		value |= ((uint32_t)ptr[3]) << 24;
		/* FALLTHRU */
	case 3:
		value |= ((uint32_t)ptr[2]) << 16;
		/* FALLTHRU */
	case 2:
		value |= ptr[1] << 8;
		/* FALLTHRU */
	case 1:
		value |= ptr[0];
		/* FALLTHRU */
	case 0:
		break;
	}
	return value;
#endif
}

inline void LogicSegment::pack_sample(uint8_t *ptr, uint64_t value)
{
#ifdef HAVE_UNALIGNED_LITTLE_ENDIAN_ACCESS
	*(uint64_t*)ptr = value;
#else
	switch (unit_size_) {
	default:
		ptr[7] = value >> 56;
		/* FALLTHRU */
	case 7:
		ptr[6] = value >> 48;
		/* FALLTHRU */
	case 6:
		ptr[5] = value >> 40;
		/* FALLTHRU */
	case 5:
		ptr[4] = value >> 32;
		/* FALLTHRU */
	case 4:
		ptr[3] = value >> 24;
		/* FALLTHRU */
	case 3:
		ptr[2] = value >> 16;
		/* FALLTHRU */
	case 2:
		ptr[1] = value >> 8;
		/* FALLTHRU */
	case 1:
		ptr[0] = value;
		/* FALLTHRU */
	case 0:
		break;
	}
#endif
}

void LogicSegment::append_payload(shared_ptr<sigrok::Logic> logic)
{
	assert(unit_size_ == logic->unit_size());
	assert((logic->data_length() % unit_size_) == 0);

	append_payload(logic->data_pointer(), logic->data_length());
}

void LogicSegment::append_payload(void *data, uint64_t data_size)
{
	assert(unit_size_ > 0);
	assert((data_size % unit_size_) == 0);

	lock_guard<recursive_mutex> lock(mutex_);

	const uint64_t prev_sample_count = sample_count_;
	const uint64_t sample_count = data_size / unit_size_;

	append_samples(data, sample_count);

	// Generate the first mip-map from the data
	append_payload_to_mipmap();

	if (sample_count > 1)
		owner_.notify_samples_added(SharedPtrToSegment(shared_from_this()),
			prev_sample_count + 1, prev_sample_count + 1 + sample_count);
	else
		owner_.notify_samples_added(SharedPtrToSegment(shared_from_this()),
			prev_sample_count + 1, prev_sample_count + 1);
}

void LogicSegment::append_subsignal_payload(unsigned int index, void *data,
	uint64_t data_size, vector<uint8_t>& destination)
{
	if (index == 0)
		destination.resize(data_size * unit_size_, 0);

	// Set the bits for this sub-signal where needed
	// Note: the bytes in *data must either be 0 or 1, nothing else
	unsigned int index_byte_offs = index / 8;
	uint8_t* output_data = destination.data() + index_byte_offs;
	uint8_t* input_data = (uint8_t*)data;

	for (uint64_t i = 0; i < data_size; i++) {
		assert((i * unit_size_ + index_byte_offs) < destination.size());
		*output_data |= (input_data[i] << index);
		output_data += unit_size_;
	}

	if (index == owner_.num_channels() - 1) {
		// We gathered sample data of all sub-signals, let's append it
		append_payload(destination.data(), destination.size());
		destination.clear();
	}
}

void LogicSegment::get_samples(int64_t start_sample,
	int64_t end_sample, uint8_t* dest) const
{
	assert(start_sample >= 0);
	assert(start_sample <= (int64_t)sample_count_);
	assert(end_sample >= 0);
	assert(end_sample <= (int64_t)sample_count_);
	assert(start_sample <= end_sample);
	assert(dest != nullptr);

	lock_guard<recursive_mutex> lock(mutex_);

	get_raw_samples(start_sample, (end_sample - start_sample), dest);
}

void LogicSegment::get_subsampled_edges(
	vector<EdgePair> &edges,
	uint64_t start, uint64_t end,
	float min_length, int sig_index, bool first_change_only)
{
	uint64_t index = start;
	unsigned int level;
	bool last_sample;
	bool fast_forward;

	assert(start <= end);
	assert(min_length > 0);
	assert(sig_index >= 0);
	assert(sig_index < 64);

	lock_guard<recursive_mutex> lock(mutex_);

	// Make sure we only process as many samples as we have
	if (end > get_sample_count())
		end = get_sample_count();

	const uint64_t block_length = (uint64_t)max(min_length, 1.0f);
	const unsigned int min_level = max((int)floorf(logf(min_length) /
		LogMipMapScaleFactor) - 1, 0);
	const uint64_t sig_mask = 1ULL << sig_index;

	// Store the initial state
	last_sample = (get_unpacked_sample(start) & sig_mask) != 0;
	if (!first_change_only)
		edges.emplace_back(index++, last_sample);

	while (index + block_length <= end) {
		//----- Continue to search -----//
		level = min_level;

		// We cannot fast-forward if there is no mip-map data at
		// the minimum level.
		fast_forward = (mip_map_[level].data != nullptr);

		if (min_length < MipMapScaleFactor) {
			// Search individual samples up to the beginning of
			// the next first level mip map block
			const uint64_t final_index = min(end, pow2_ceil(index, MipMapScalePower));

			for (; index < final_index &&
					(index & ~((uint64_t)(~0) << MipMapScalePower)) != 0;
					index++) {

				const bool sample = (get_unpacked_sample(index) & sig_mask) != 0;

				// If there was a change we cannot fast forward
				if (sample != last_sample) {
					fast_forward = false;
					break;
				}
			}
		} else {
			// If resolution is less than a mip map block,
			// round up to the beginning of the mip-map block
			// for this level of detail
			const int min_level_scale_power = (level + 1) * MipMapScalePower;
			index = pow2_ceil(index, min_level_scale_power);
			if (index >= end)
				break;

			// We can fast forward only if there was no change
			const bool sample = (get_unpacked_sample(index) & sig_mask) != 0;
			if (last_sample != sample)
				fast_forward = false;
		}

		if (fast_forward) {

			// Fast forward: This involves zooming out to higher
			// levels of the mip map searching for changes, then
			// zooming in on them to find the point where the edge
			// begins.

			// Slide right and zoom out at the beginnings of mip-map
			// blocks until we encounter a change
			while (true) {
				const int level_scale_power = (level + 1) * MipMapScalePower;
				const uint64_t offset = index >> level_scale_power;

				// Check if we reached the last block at this
				// level, or if there was a change in this block
				if (offset >= mip_map_[level].length ||
					(get_subsample(level, offset) &	sig_mask))
					break;

				if ((offset & ~((uint64_t)(~0) << MipMapScalePower)) == 0) {
					// If we are now at the beginning of a
					// higher level mip-map block ascend one
					// level
					if ((level + 1 >= ScaleStepCount) || (!mip_map_[level + 1].data))
						break;

					level++;
				} else {
					// Slide right to the beginning of the
					// next mip map block
					index = pow2_ceil(index + 1, level_scale_power);
				}
			}

			// Zoom in, and slide right until we encounter a change,
			// and repeat until we reach min_level
			while (true) {
				assert(mip_map_[level].data);

				const int level_scale_power = (level + 1) * MipMapScalePower;
				const uint64_t offset = index >> level_scale_power;

				// Check if we reached the last block at this
				// level, or if there was a change in this block
				if (offset >= mip_map_[level].length ||
						(get_subsample(level, offset) & sig_mask)) {
					// Zoom in unless we reached the minimum
					// zoom
					if (level == min_level)
						break;

					level--;
				} else {
					// Slide right to the beginning of the
					// next mip map block
					index = pow2_ceil(index + 1, level_scale_power);
				}
			}

			// If individual samples within the limit of resolution,
			// do a linear search for the next transition within the
			// block
			if (min_length < MipMapScaleFactor) {
				for (; index < end; index++) {
					const bool sample = (get_unpacked_sample(index) & sig_mask) != 0;
					if (sample != last_sample)
						break;
				}
			}
		}

		//----- Store the edge -----//

		// Take the last sample of the quanization block
		const int64_t final_index = index + block_length;
		if (index + block_length > end)
			break;

		// Store the final state
		const bool final_sample = (get_unpacked_sample(final_index - 1) & sig_mask) != 0;
		edges.emplace_back(index, final_sample);

		index = final_index;
		last_sample = final_sample;

		if (first_change_only)
			break;
	}

	// Add the final state
	if (!first_change_only) {
		const bool end_sample = get_unpacked_sample(end) & sig_mask;
		if (last_sample != end_sample)
			edges.emplace_back(end, end_sample);
		edges.emplace_back(end + 1, end_sample);
	}
}

void LogicSegment::get_surrounding_edges(vector<EdgePair> &dest,
	uint64_t origin_sample, float min_length, int sig_index)
{
	if (origin_sample >= sample_count_)
		return;

	// Put the edges vector on the heap, it can become quite big until we can
	// use a get_subsampled_edges() implementation that searches backwards
	vector<EdgePair>* edges = new vector<EdgePair>;

	// Get all edges to the left of origin_sample
	get_subsampled_edges(*edges, 0, origin_sample, min_length, sig_index, false);

	// If we don't specify "first only", the first and last edge are the states
	// at samples 0 and origin_sample. If only those exist, there are no edges
	if (edges->size() == 2) {
		delete edges;
		return;
	}

	// Dismiss the entry for origin_sample so that back() gives us the
	// real last entry
	edges->pop_back();
	dest.push_back(edges->back());
	edges->clear();

	// Get first edge to the right of origin_sample
	get_subsampled_edges(*edges, origin_sample, sample_count_, min_length, sig_index, true);

	// "first only" is specified, so nothing needs to be dismissed
	if (edges->size() == 0) {
		delete edges;
		return;
	}

	dest.push_back(edges->front());

	delete edges;
}

void LogicSegment::reallocate_mipmap_level(MipMapLevel &m)
{
	lock_guard<recursive_mutex> lock(mutex_);

	const uint64_t new_data_length = ((m.length + MipMapDataUnit - 1) /
		MipMapDataUnit) * MipMapDataUnit;

	if (new_data_length > m.data_length) {
		m.data_length = new_data_length;

		// Padding is added to allow for the uint64_t write word
		m.data = realloc(m.data, new_data_length * unit_size_ +
			sizeof(uint64_t));
	}
}

void LogicSegment::append_payload_to_mipmap()
{
	MipMapLevel &m0 = mip_map_[0];
	uint64_t prev_length;
	uint8_t *dest_ptr;
	SegmentDataIterator* it;
	uint64_t accumulator;
	unsigned int diff_counter;

	// Expand the data buffer to fit the new samples
	prev_length = m0.length;
	m0.length = sample_count_ / MipMapScaleFactor;

	// Break off if there are no new samples to compute
	if (m0.length == prev_length)
		return;

	reallocate_mipmap_level(m0);

	dest_ptr = (uint8_t*)m0.data + prev_length * unit_size_;

	// Iterate through the samples to populate the first level mipmap
	const uint64_t start_sample = prev_length * MipMapScaleFactor;
	const uint64_t end_sample = m0.length * MipMapScaleFactor;
	uint64_t len_sample = end_sample - start_sample;
	it = begin_sample_iteration(start_sample);
	while (len_sample > 0) {
		// Number of samples available in this chunk
		uint64_t count = get_iterator_valid_length(it);
		// Reduce if less than asked for
		count = std::min(count, len_sample);
		uint8_t *src_ptr = get_iterator_value(it);
		// Submit these contiguous samples to downsampling in bulk
		if (unit_size_ == 1)
			downsampleT<uint8_t>(src_ptr, dest_ptr, count);
		else if (unit_size_ == 2)
			downsampleT<uint16_t>(src_ptr, dest_ptr, count);
		else if (unit_size_ == 4)
			downsampleT<uint32_t>(src_ptr, dest_ptr, count);
		else if (unit_size_ == 8)
			downsampleT<uint64_t>(src_ptr, dest_ptr, count);
		else
			downsampleGeneric(src_ptr, dest_ptr, count);
		len_sample -= count;
		// Advance iterator, should move to start of next chunk
		continue_sample_iteration(it, count);
	}
	end_sample_iteration(it);

	// Compute higher level mipmaps
	for (unsigned int level = 1; level < ScaleStepCount; level++) {
		MipMapLevel &m = mip_map_[level];
		const MipMapLevel &ml = mip_map_[level - 1];

		// Expand the data buffer to fit the new samples
		prev_length = m.length;
		m.length = ml.length / MipMapScaleFactor;

		// Break off if there are no more samples to be computed
		if (m.length == prev_length)
			break;

		reallocate_mipmap_level(m);

		// Subsample the lower level
		const uint8_t* src_ptr = (uint8_t*)ml.data +
			unit_size_ * prev_length * MipMapScaleFactor;
		const uint8_t *const end_dest_ptr =
			(uint8_t*)m.data + unit_size_ * m.length;

		for (dest_ptr = (uint8_t*)m.data +
				unit_size_ * prev_length;
				dest_ptr < end_dest_ptr;
				dest_ptr += unit_size_) {
			accumulator = 0;
			diff_counter = MipMapScaleFactor;
			while (diff_counter-- > 0) {
				accumulator |= unpack_sample(src_ptr);
				src_ptr += unit_size_;
			}

			pack_sample(dest_ptr, accumulator);
		}
	}
}

uint64_t LogicSegment::get_unpacked_sample(uint64_t index) const
{
	assert(index < sample_count_);

	assert(unit_size_ <= 8);  // 8 * 8 = 64 channels
	uint8_t data[8];

	get_raw_samples(index, 1, data);

	return unpack_sample(data);
}

uint64_t LogicSegment::get_subsample(int level, uint64_t offset) const
{
	assert(level >= 0);
	assert(mip_map_[level].data);
	return unpack_sample((uint8_t*)mip_map_[level].data +
		unit_size_ * offset);
}

uint64_t LogicSegment::pow2_ceil(uint64_t x, unsigned int power)
{
	const uint64_t p = UINT64_C(1) << power;
	return (x + p - 1) / p * p;
}

} // namespace data
} // namespace pv
