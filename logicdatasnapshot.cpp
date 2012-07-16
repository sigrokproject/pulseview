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

#include "extdef.h"

#include <assert.h>
#include <string.h>
#include <math.h>

#include <boost/foreach.hpp>

#include "logicdatasnapshot.h"

using namespace std;

const int LogicDataSnapshot::MipMapScalePower = 4;
const int LogicDataSnapshot::MipMapScaleFactor = 1 << MipMapScalePower;
const uint64_t LogicDataSnapshot::MipMapDataUnit = 64*1024;	// bytes

LogicDataSnapshot::LogicDataSnapshot(
	const sr_datafeed_logic &logic) :
	DataSnapshot(logic.unitsize),
	_last_append_sample(0)
{
	memset(_mip_map, 0, sizeof(_mip_map));
	append_payload(logic);
}

LogicDataSnapshot::~LogicDataSnapshot()
{
	BOOST_FOREACH(MipMapLevel &l, _mip_map)
		free(l.data);
}

void LogicDataSnapshot::append_payload(
	const sr_datafeed_logic &logic)
{
	assert(_unit_size == logic.unitsize);

	const uint64_t prev_length = _data_length;
	append_data(logic.data, logic.length);

	// Generate the first mip-map from the data
	append_payload_to_mipmap();
}

void LogicDataSnapshot::reallocate_mip_map(MipMapLevel &m)
{
	const uint64_t new_data_length = ((m.length + MipMapDataUnit - 1) /
		MipMapDataUnit) * MipMapDataUnit;
	if(new_data_length > m.data_length)
	{
		m.data_length = new_data_length;
		m.data = realloc(m.data, new_data_length * _unit_size);
	}
}

void LogicDataSnapshot::append_payload_to_mipmap()
{
	MipMapLevel &m0 = _mip_map[0];
	uint64_t prev_length;
	const uint8_t *src_ptr;
	uint8_t *dest_ptr;
	uint64_t accumulator;
	unsigned int diff_counter;

	// Expand the data buffer to fit the new samples
	prev_length = m0.length;
	m0.length = _data_length / MipMapScaleFactor;

	// Break off if there are no new samples to compute
	if(m0.length == prev_length)
		return;

	reallocate_mip_map(m0);

	dest_ptr = (uint8_t*)m0.data + prev_length * _unit_size;

	// Iterate through the samples to populate the first level mipmap
	accumulator = 0;
	diff_counter = MipMapScaleFactor;
	const uint8_t *end_src_ptr = (uint8_t*)_data +
		m0.length * _unit_size * MipMapScaleFactor;
	for(src_ptr = (uint8_t*)_data +
		prev_length * _unit_size * MipMapScaleFactor;
		src_ptr < end_src_ptr;)
	{
		// Accumulate transitions which have occurred in this sample
		accumulator = 0;
		diff_counter = MipMapScaleFactor;
		while(diff_counter-- > 0)
		{
			const uint64_t sample = *(uint64_t*)src_ptr;
			accumulator |= _last_append_sample ^ sample;
			_last_append_sample = sample;
			src_ptr += _unit_size;
		}

		*(uint64_t*)dest_ptr = accumulator;
		dest_ptr += _unit_size;
	}

	// Compute higher level mipmaps
	for(int level = 1; level < ScaleStepCount; level++)
	{
		MipMapLevel &m = _mip_map[level];
		const MipMapLevel &ml = _mip_map[level-1];

		// Expand the data buffer to fit the new samples
		prev_length = m.length;
		m.length = ml.length / MipMapScaleFactor;

		// Break off if there are no more samples to computed
		if(m.length == prev_length)
			break;

		reallocate_mip_map(m);

		// Subsample the level lower level
		src_ptr = (uint8_t*)ml.data +
			_unit_size * prev_length * MipMapScaleFactor;
		const uint8_t *end_dest_ptr =
			(uint8_t*)m.data + _unit_size * m.length;
		for(dest_ptr = (uint8_t*)m.data +
			_unit_size * prev_length;
			dest_ptr < end_dest_ptr;
			dest_ptr += _unit_size)
		{
			accumulator = 0;
			diff_counter = MipMapScaleFactor;
			while(diff_counter-- > 0)
			{
				accumulator |= *(uint64_t*)src_ptr;
				src_ptr += _unit_size;
			}

			*(uint64_t*)dest_ptr = accumulator;
		}
	}
}

uint64_t LogicDataSnapshot::get_sample(uint64_t index) const
{
	assert(_data);
	assert(index >= 0 && index < _data_length);

	return *(uint64_t*)((uint8_t*)_data + index * _unit_size);
}

void LogicDataSnapshot::get_subsampled_edges(
	std::vector<EdgePair> &edges,
	int64_t start, int64_t end,
	int64_t quantization_length, int sig_index)
{
	assert(start >= 0);
	assert(end < get_sample_count());
	assert(start <= end);
	assert(quantization_length > 0);
	assert(sig_index >= 0);
	assert(sig_index < SR_MAX_NUM_PROBES);

	const uint64_t sig_mask = 1 << sig_index;

	// Add the initial state
	bool last_sample = get_sample(start) & sig_mask;
	edges.push_back(pair<int64_t, bool>(start, last_sample));

	for(int64_t i = start + 1; i < end; i++)
	{
		const bool sample = get_sample(i) & sig_mask;

		// Check if we hit an edge
		if(sample != last_sample)
		{
			// Take the last sample of the quanization block
			const int64_t final_index =
				min((i - (i % quantization_length) +
				quantization_length - 1), end);

			// Store the final state
			const bool final_sample = get_sample(final_index) & sig_mask;
			edges.push_back(pair<int64_t, bool>(
				final_index, final_sample));

			// Continue to sampling
			i = final_index;
			last_sample = final_sample;
		}
	}

	// Add the final state
	edges.push_back(pair<int64_t, bool>(end,
		get_sample(end) & sig_mask));
}
