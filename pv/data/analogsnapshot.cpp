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

#include <boost/foreach.hpp>

#include "analogsnapshot.h"

using namespace boost;
using namespace std;

namespace pv {
namespace data {

const int AnalogSnapshot::EnvelopeScalePower = 4;
const int AnalogSnapshot::EnvelopeScaleFactor = 1 << EnvelopeScalePower;
const float AnalogSnapshot::LogEnvelopeScaleFactor =
	logf(EnvelopeScaleFactor);
const uint64_t AnalogSnapshot::EnvelopeDataUnit = 64*1024;	// bytes

AnalogSnapshot::AnalogSnapshot(const sr_datafeed_analog &analog) :
	Snapshot(sizeof(float))
{
	lock_guard<recursive_mutex> lock(_mutex);
	memset(_envelope_levels, 0, sizeof(_envelope_levels));
	append_payload(analog);
}

AnalogSnapshot::~AnalogSnapshot()
{
	lock_guard<recursive_mutex> lock(_mutex);
	BOOST_FOREACH(Envelope &e, _envelope_levels)
		free(e.samples);
}

void AnalogSnapshot::append_payload(
	const sr_datafeed_analog &analog)
{
	lock_guard<recursive_mutex> lock(_mutex);
	append_data(analog.data, analog.num_samples);

	// Generate the first mip-map from the data
	append_payload_to_envelope_levels();
}

const float* AnalogSnapshot::get_samples(
	int64_t start_sample, int64_t end_sample) const
{
	assert(start_sample >= 0);
	assert(start_sample < (int64_t)_sample_count);
	assert(end_sample >= 0);
	assert(end_sample < (int64_t)_sample_count);
	assert(start_sample <= end_sample);

	lock_guard<recursive_mutex> lock(_mutex);

	float *const data = new float[end_sample - start_sample];
	memcpy(data, (float*)_data + start_sample, sizeof(float) *
		(end_sample - start_sample));
	return data;
}

void AnalogSnapshot::reallocate_envelope(Envelope &e)
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

void AnalogSnapshot::append_payload_to_envelope_levels()
{
	Envelope &e0 = _envelope_levels[0];
	uint64_t prev_length;
	EnvelopeSample *dest_ptr;

	// Expand the data buffer to fit the new samples
	prev_length = e0.length;
	e0.length = _sample_count / EnvelopeScaleFactor;

	// Break off if there are no new samples to compute
	if (e0.length == prev_length)
		return;

	reallocate_envelope(e0);

	dest_ptr = e0.samples + prev_length;

	// Iterate through the samples to populate the first level mipmap
	const float *const end_src_ptr = (float*)_data +
		e0.length * EnvelopeScaleFactor;
	for (const float *src_ptr = (float*)_data +
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
		Envelope &e = _envelope_levels[level];
		const Envelope &el = _envelope_levels[level-1];

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
