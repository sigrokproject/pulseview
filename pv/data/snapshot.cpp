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

#include "snapshot.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

using std::lock_guard;
using std::recursive_mutex;

namespace pv {
namespace data {

Snapshot::Snapshot(int unit_size) :
	_data(NULL),
	_sample_count(0),
	_capacity(0),
	_unit_size(unit_size)
{
	lock_guard<recursive_mutex> lock(_mutex);
	assert(_unit_size > 0);
}

Snapshot::~Snapshot()
{
	lock_guard<recursive_mutex> lock(_mutex);
	free(_data);
}

uint64_t Snapshot::get_sample_count() const
{
	lock_guard<recursive_mutex> lock(_mutex);
	return _sample_count;
}

int Snapshot::unit_size() const
{
	return _unit_size;
}

void Snapshot::set_capacity(const uint64_t new_capacity)
{
	lock_guard<recursive_mutex> lock(_mutex);

	assert(_capacity >= _sample_count);
	if (new_capacity > _capacity) {
		_capacity = new_capacity;
		_data = realloc(_data, (new_capacity * _unit_size) + sizeof(uint64_t));
	}
}

uint64_t Snapshot::capacity() const
{
	lock_guard<recursive_mutex> lock(_mutex);
	return _capacity;
}

void Snapshot::append_data(void *data, uint64_t samples)
{
	lock_guard<recursive_mutex> lock(_mutex);

	assert(_capacity >= _sample_count);

	// Ensure there's enough capacity to copy.
	const uint64_t free_space = _capacity - _sample_count;
	if (free_space < samples) {
		set_capacity(_sample_count + samples);
	}

	memcpy((uint8_t*)_data + _sample_count * _unit_size,
		data, samples * _unit_size);
	_sample_count += samples;
}

} // namespace data
} // namespace pv
