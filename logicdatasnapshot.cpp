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

#include "logicdatasnapshot.h"

#include <assert.h>

using namespace std;

LogicDataSnapshot::LogicDataSnapshot(
	const sr_datafeed_logic &logic) :
	DataSnapshot(logic.unitsize)
{
	append_payload(logic);
}

void LogicDataSnapshot::append_payload(
	const sr_datafeed_logic &logic)
{
	assert(_unit_size == logic.unitsize);

	append_data(logic.data, logic.length);
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

	for(int64_t i = start + 1; i < end - 1; i++)
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
	edges.push_back(pair<int64_t, bool>(end - 1,
		get_sample(end - 1) & sig_mask));
}
