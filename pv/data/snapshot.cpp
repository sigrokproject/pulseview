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

#include "snapshot.hpp"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

using std::lock_guard;
using std::recursive_mutex;

namespace pv {
namespace data {

Snapshot::Snapshot(uint64_t samplerate, unsigned int unit_size) :
	sample_count_(0),
	start_time_(0),
	samplerate_(samplerate),
	capacity_(0),
	unit_size_(unit_size)
{
	lock_guard<recursive_mutex> lock(mutex_);
	assert(unit_size_ > 0);
}

Snapshot::~Snapshot()
{
	lock_guard<recursive_mutex> lock(mutex_);
}

uint64_t Snapshot::get_sample_count() const
{
	lock_guard<recursive_mutex> lock(mutex_);
	return sample_count_;
}

double Snapshot::start_time() const
{
	return start_time_;
}

double Snapshot::samplerate() const
{
	return samplerate_;
}

void Snapshot::set_samplerate(double samplerate)
{
	samplerate_ = samplerate;
}

unsigned int Snapshot::unit_size() const
{
	return unit_size_;
}

void Snapshot::set_capacity(const uint64_t new_capacity)
{
	lock_guard<recursive_mutex> lock(mutex_);

	assert(capacity_ >= sample_count_);
	if (new_capacity > capacity_) {
		capacity_ = new_capacity;
		data_.resize((new_capacity * unit_size_) + sizeof(uint64_t));
	}
}

uint64_t Snapshot::capacity() const
{
	lock_guard<recursive_mutex> lock(mutex_);
	return data_.size();
}

void Snapshot::append_data(void *data, uint64_t samples)
{
	lock_guard<recursive_mutex> lock(mutex_);

	assert(capacity_ >= sample_count_);

	// Ensure there's enough capacity to copy.
	const uint64_t free_space = capacity_ - sample_count_;
	if (free_space < samples) {
		set_capacity(sample_count_ + samples);
	}

	memcpy((uint8_t*)data_.data() + sample_count_ * unit_size_,
		data, samples * unit_size_);
	sample_count_ += samples;
}

} // namespace data
} // namespace pv
