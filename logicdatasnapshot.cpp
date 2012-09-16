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
const float LogicDataSnapshot::LogMipMapScaleFactor = logf(MipMapScaleFactor);
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
	float min_length, int sig_index)
{
	int64_t index;
	int level;

	assert(start >= 0);
	assert(end <= get_sample_count());
	assert(start <= end);
	assert(min_length > 0);
	assert(sig_index >= 0);
	assert(sig_index < SR_MAX_NUM_PROBES);

	const int min_level = max((int)floorf(logf(min_length) /
		LogMipMapScaleFactor) - 1, 0);
	const uint64_t sig_mask = 1 << sig_index;

	// Add the initial state
	bool last_sample = get_sample(start) & sig_mask;
	edges.push_back(pair<int64_t, bool>(start, last_sample));

	index = start + 1;
	for(index = start + 1; index < end;)
	{
		level = min_level;

		if(min_length < MipMapScaleFactor)
		{
			// Search individual samples up to the beginning of
			// the next first level mip map block
			const uint64_t final_sample = min(end,
				pow2_ceil(index, MipMapScalePower));

			for(index;
				index < final_sample &&
				(index & ~(~0 << MipMapScalePower)) != 0;
				index++)
			{
				const bool sample =
					(get_sample(index) & sig_mask) != 0;
				if(sample != last_sample)
					break;
			}
		}
		else
		{
			// If resolution is less than a mip map block,
			// round up to the beginning of the mip-map block
			// for this level of detail
			const int min_level_scale_power =
				(level + 1) * MipMapScalePower;
			index = pow2_ceil(index, min_level_scale_power);
		}

		// Slide right and zoom out at the beginnings of mip-map
		// blocks until we encounter a change
		while(1)
		{
			const int level_scale_power =
				(level + 1) * MipMapScalePower;
			const uint64_t offset = index >> level_scale_power;
			assert(offset >= 0);

			// Check if we reached the last block at this level,
			// or if there was a change in this block
			if(offset >= _mip_map[level].length ||
				(get_subsample(level, offset) & sig_mask))
				break;

			if((offset & ~(~0 << MipMapScalePower)) == 0)
			{
				// If we are now at the beginning of a higher
				// level mip-map block ascend one level
				if(!_mip_map[level + 1].data)
					break;

				level++;
			}
			else
			{
				// Slide right to the beginning of the next mip
				// map block
				index = pow2_ceil(index, level_scale_power);
			}
		}

		// Zoom in, and slide right until we encounter a change,
		// and repeat until we reach min_level
		while(1)
		{
			assert(_mip_map[level].data);

			const int level_scale_power =
				(level + 1) * MipMapScalePower;
			const uint64_t offset = index >> level_scale_power;
			assert(offset >= 0);

			// Check if we reached the last block at this level,
			// or if there was a change in this block
			if(offset >= _mip_map[level].length ||
				(get_subsample(level, offset) & sig_mask))
			{
				// Zoom in unless we reached the minimum zoom
				if(level == min_level)
					break;

				level--;
			}
			else
			{
				// Slide right to the beginning of the next mip map block
				index = pow2_ceil(index, level_scale_power);
			}
		}

		// If individual samples within the limit of resolution,
		// do a linear search for the next transition within the block
		if(min_length < MipMapScaleFactor)
		{
			for(index; index < end; index++)
			{
				const bool sample =
					(get_sample(index) & sig_mask) != 0;
				if(sample != last_sample)
					break;
			}
		}

		if(index < end)
		{
			// Take the last sample of the quanization block
			const int64_t block_length = (int64_t)max(min_length, 1.0f);
			const int64_t rem = index % block_length;
			const int64_t final_index = min(index + (rem == 0 ? 0 :
				block_length - rem), end);

			// Store the final state
			const bool final_sample = get_sample(final_index) & sig_mask;
			edges.push_back(pair<int64_t, bool>(
				final_index, final_sample));

			// Continue to sample
			index = final_index;
			last_sample = final_sample;

			index++;
		}
	}

	// Add the final state
	edges.push_back(pair<int64_t, bool>(end,
		get_sample(end) & sig_mask));
}

uint64_t LogicDataSnapshot::get_subsample(int level, uint64_t offset) const
{
	assert(level >= 0);
	assert(_mip_map[level].data);
	return *(uint64_t*)((uint8_t*)_mip_map[level].data +
		_unit_size * offset);
}

int64_t LogicDataSnapshot::pow2_ceil(int64_t x, unsigned int power)
{
	const int64_t p = 1 << power;
	return ((x < 0) ? x : (x + p - 1)) / p * p;
}
