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

#include <extdef.h>

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>

#include <algorithm>

#include "analog.hpp"
#include "analogsegment.hpp"

using std::lock_guard;
using std::recursive_mutex;
using std::make_pair;
using std::max;
using std::max_element;
using std::min;
using std::min_element;
using std::pair;
using std::unique_ptr;

namespace pv {
namespace data {

const int AnalogSegment::EnvelopeScalePower = 4;
const int AnalogSegment::EnvelopeScaleFactor = 1 << EnvelopeScalePower;
const float AnalogSegment::LogEnvelopeScaleFactor = logf(EnvelopeScaleFactor);
const uint64_t AnalogSegment::EnvelopeDataUnit = 64 * 1024;	// bytes

AnalogSegment::AnalogSegment(Analog& owner, uint32_t segment_id, uint64_t samplerate) :
	Segment(segment_id, samplerate, sizeof(float)),
	owner_(owner),
	min_value_(0),
	max_value_(0)
{
	lock_guard<recursive_mutex> lock(mutex_);
	memset(envelope_levels_, 0, sizeof(envelope_levels_));
}

AnalogSegment::~AnalogSegment()
{
	lock_guard<recursive_mutex> lock(mutex_);
	for (Envelope &e : envelope_levels_)
		free(e.samples);
}

void AnalogSegment::append_interleaved_samples(const float *data,
	size_t sample_count, size_t stride)
{
	assert(unit_size_ == sizeof(float));

	lock_guard<recursive_mutex> lock(mutex_);

	uint64_t prev_sample_count = sample_count_;

	// Deinterleave the samples and add them
	unique_ptr<float[]> deint_data(new float[sample_count]);
	float *deint_data_ptr = deint_data.get();
	for (uint32_t i = 0; i < sample_count; i++) {
		*deint_data_ptr = (float)(*data);
		deint_data_ptr++;
		data += stride;
	}

	append_samples(deint_data.get(), sample_count);

	// Generate the first mip-map from the data
	append_payload_to_envelope_levels();

	if (sample_count > 1)
		owner_.notify_samples_added(this, prev_sample_count + 1,
			prev_sample_count + 1 + sample_count);
	else
		owner_.notify_samples_added(this, prev_sample_count + 1,
			prev_sample_count + 1);
}

void AnalogSegment::get_samples(int64_t start_sample, int64_t end_sample,
	float* dest) const
{
	assert(start_sample >= 0);
	assert(start_sample < (int64_t)sample_count_);
	assert(end_sample >= 0);
	assert(end_sample <= (int64_t)sample_count_);
	assert(start_sample <= end_sample);
	assert(dest != nullptr);

	lock_guard<recursive_mutex> lock(mutex_);

	get_raw_samples(start_sample, (end_sample - start_sample), (uint8_t*)dest);
}

const pair<float, float> AnalogSegment::get_min_max() const
{
	return make_pair(min_value_, max_value_);
}

float* AnalogSegment::get_iterator_value_ptr(SegmentDataIterator* it)
{
	assert(it->sample_index <= (sample_count_ - 1));

	return (float*)(it->chunk + it->chunk_offs);
}

void AnalogSegment::get_envelope_section(EnvelopeSection &s,
	uint64_t start, uint64_t end, float min_length) const
{
	assert(end <= get_sample_count());
	assert(start <= end);
	assert(min_length > 0);

	lock_guard<recursive_mutex> lock(mutex_);

	const unsigned int min_level = max((int)floorf(logf(min_length) /
		LogEnvelopeScaleFactor) - 1, 0);
	const unsigned int scale_power = (min_level + 1) *
		EnvelopeScalePower;
	start >>= scale_power;
	end >>= scale_power;

	s.start = start << scale_power;
	s.scale = 1 << scale_power;
	s.length = end - start;
	s.samples = new EnvelopeSample[s.length];
	memcpy(s.samples, envelope_levels_[min_level].samples + start,
		s.length * sizeof(EnvelopeSample));
}

void AnalogSegment::reallocate_envelope(Envelope &e)
{
	const uint64_t new_data_length = ((e.length + EnvelopeDataUnit - 1) /
		EnvelopeDataUnit) * EnvelopeDataUnit;
	if (new_data_length > e.data_length) {
		e.data_length = new_data_length;
		e.samples = (EnvelopeSample*)realloc(e.samples,
			new_data_length * sizeof(EnvelopeSample));
	}
}

void AnalogSegment::append_payload_to_envelope_levels()
{
	Envelope &e0 = envelope_levels_[0];
	uint64_t prev_length;
	EnvelopeSample *dest_ptr;
	SegmentDataIterator* it;

	// Expand the data buffer to fit the new samples
	prev_length = e0.length;
	e0.length = sample_count_ / EnvelopeScaleFactor;

	// Calculate min/max values in case we have too few samples for an envelope
	const float old_min_value = min_value_, old_max_value = max_value_;
	if (sample_count_ < EnvelopeScaleFactor) {
		it = begin_sample_iteration(0);
		for (uint64_t i = 0; i < sample_count_; i++) {
			const float sample = *get_iterator_value_ptr(it);
			if (sample < min_value_)
				min_value_ = sample;
			if (sample > max_value_)
				max_value_ = sample;
			continue_sample_iteration(it, 1);
		}
		end_sample_iteration(it);
	}

	// Break off if there are no new samples to compute
	if (e0.length == prev_length)
		return;

	reallocate_envelope(e0);

	dest_ptr = e0.samples + prev_length;

	// Iterate through the samples to populate the first level mipmap
	uint64_t start_sample = prev_length * EnvelopeScaleFactor;
	uint64_t end_sample = e0.length * EnvelopeScaleFactor;

	it = begin_sample_iteration(start_sample);
	for (uint64_t i = start_sample; i < end_sample; i += EnvelopeScaleFactor) {
		const float* samples = get_iterator_value_ptr(it);

		const EnvelopeSample sub_sample = {
			*min_element(samples, samples + EnvelopeScaleFactor),
			*max_element(samples, samples + EnvelopeScaleFactor),
		};

		if (sub_sample.min < min_value_)
			min_value_ = sub_sample.min;
		if (sub_sample.max > max_value_)
			max_value_ = sub_sample.max;

		continue_sample_iteration(it, EnvelopeScaleFactor);
		*dest_ptr++ = sub_sample;
	}
	end_sample_iteration(it);

	// Compute higher level mipmaps
	for (unsigned int level = 1; level < ScaleStepCount; level++) {
		Envelope &e = envelope_levels_[level];
		const Envelope &el = envelope_levels_[level - 1];

		// Expand the data buffer to fit the new samples
		prev_length = e.length;
		e.length = el.length / EnvelopeScaleFactor;

		// Break off if there are no more samples to be computed
		if (e.length == prev_length)
			break;

		reallocate_envelope(e);

		// Subsample the lower level
		const EnvelopeSample *src_ptr =
			el.samples + prev_length * EnvelopeScaleFactor;
		const EnvelopeSample *const end_dest_ptr = e.samples + e.length;

		for (dest_ptr = e.samples + prev_length;
				dest_ptr < end_dest_ptr; dest_ptr++) {
			const EnvelopeSample *const end_src_ptr =
				src_ptr + EnvelopeScaleFactor;

			EnvelopeSample sub_sample = *src_ptr++;
			while (src_ptr < end_src_ptr) {
				sub_sample.min = min(sub_sample.min, src_ptr->min);;
				sub_sample.max = max(sub_sample.max, src_ptr->max);
				src_ptr++;
			}

			*dest_ptr = sub_sample;
		}
	}

	// Notify if the min or max value changed
	if ((old_min_value != min_value_) || (old_max_value != max_value_))
		Q_EMIT owner_.min_max_changed(min_value_, max_value_);
}

} // namespace data
} // namespace pv
