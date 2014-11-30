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

#include <extdef.h>

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <algorithm>

#include "analogsegment.hpp"

using std::lock_guard;
using std::recursive_mutex;
using std::max;
using std::max_element;
using std::min;
using std::min_element;

namespace pv {
namespace data {

const int AnalogSegment::EnvelopeScalePower = 4;
const int AnalogSegment::EnvelopeScaleFactor = 1 << EnvelopeScalePower;
const float AnalogSegment::LogEnvelopeScaleFactor =
	logf(EnvelopeScaleFactor);
const uint64_t AnalogSegment::EnvelopeDataUnit = 64*1024;	// bytes

AnalogSegment::AnalogSegment(
	uint64_t samplerate, const uint64_t expected_num_samples) :
	Segment(samplerate, sizeof(float))
{
	set_capacity(expected_num_samples);

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

	data_.resize((sample_count_ + sample_count) * sizeof(float));

	float *dst = (float*)data_.data() + sample_count_;
	const float *dst_end = dst + sample_count;
	while (dst != dst_end)
	{
		*dst++ = *data;
		data += stride;
	}

	sample_count_ += sample_count;

	// Generate the first mip-map from the data
	append_payload_to_envelope_levels();
}

const float* AnalogSegment::get_samples(
	int64_t start_sample, int64_t end_sample) const
{
	assert(start_sample >= 0);
	assert(start_sample < (int64_t)sample_count_);
	assert(end_sample >= 0);
	assert(end_sample < (int64_t)sample_count_);
	assert(start_sample <= end_sample);

	lock_guard<recursive_mutex> lock(mutex_);

	float *const data = new float[end_sample - start_sample];
	memcpy(data, (float*)data_.data() + start_sample, sizeof(float) *
		(end_sample - start_sample));
	return data;
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
	if (new_data_length > e.data_length)
	{
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

	// Expand the data buffer to fit the new samples
	prev_length = e0.length;
	e0.length = sample_count_ / EnvelopeScaleFactor;

	// Break off if there are no new samples to compute
	if (e0.length == prev_length)
		return;

	reallocate_envelope(e0);

	dest_ptr = e0.samples + prev_length;

	// Iterate through the samples to populate the first level mipmap
	const float *const end_src_ptr = (float*)data_.data() +
		e0.length * EnvelopeScaleFactor;
	for (const float *src_ptr = (float*)data_.data() +
		prev_length * EnvelopeScaleFactor;
		src_ptr < end_src_ptr; src_ptr += EnvelopeScaleFactor)
	{
		const EnvelopeSample sub_sample = {
			*min_element(src_ptr, src_ptr + EnvelopeScaleFactor),
			*max_element(src_ptr, src_ptr + EnvelopeScaleFactor),
		};

		*dest_ptr++ = sub_sample;
	}

	// Compute higher level mipmaps
	for (unsigned int level = 1; level < ScaleStepCount; level++)
	{
		Envelope &e = envelope_levels_[level];
		const Envelope &el = envelope_levels_[level-1];

		// Expand the data buffer to fit the new samples
		prev_length = e.length;
		e.length = el.length / EnvelopeScaleFactor;

		// Break off if there are no more samples to computed
		if (e.length == prev_length)
			break;

		reallocate_envelope(e);

		// Subsample the level lower level
		const EnvelopeSample *src_ptr =
			el.samples + prev_length * EnvelopeScaleFactor;
		const EnvelopeSample *const end_dest_ptr = e.samples + e.length;
		for (dest_ptr = e.samples + prev_length;
			dest_ptr < end_dest_ptr; dest_ptr++)
		{
			const EnvelopeSample *const end_src_ptr =
				src_ptr + EnvelopeScaleFactor;

			EnvelopeSample sub_sample = *src_ptr++;
			while (src_ptr < end_src_ptr)
			{
				sub_sample.min = min(sub_sample.min, src_ptr->min);
				sub_sample.max = max(sub_sample.max, src_ptr->max);
				src_ptr++;
			}

			*dest_ptr = sub_sample;
		}
	}
}

} // namespace data
} // namespace pv
