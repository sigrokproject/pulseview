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

#include "segment.hpp"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

using std::lock_guard;
using std::recursive_mutex;

namespace pv {
namespace data {

Segment::Segment(uint64_t samplerate, unsigned int unit_size) :
	sample_count_(0),
	start_time_(0),
	samplerate_(samplerate),
	capacity_(0),
	unit_size_(unit_size)
{
	lock_guard<recursive_mutex> lock(mutex_);
	assert(unit_size_ > 0);
}

Segment::~Segment()
{
	lock_guard<recursive_mutex> lock(mutex_);
}

uint64_t Segment::get_sample_count() const
{
	lock_guard<recursive_mutex> lock(mutex_);
	return sample_count_;
}

const pv::util::Timestamp& Segment::start_time() const
{
	return start_time_;
}

double Segment::samplerate() const
{
	return samplerate_;
}

void Segment::set_samplerate(double samplerate)
{
	samplerate_ = samplerate;
}

unsigned int Segment::unit_size() const
{
	return unit_size_;
}

void Segment::set_capacity(const uint64_t new_capacity)
{
	lock_guard<recursive_mutex> lock(mutex_);

	assert(capacity_ >= sample_count_);
	if (new_capacity > capacity_) {
		// If we're out of memory, this will throw std::bad_alloc
		data_.resize((new_capacity * unit_size_) + sizeof(uint64_t));
		capacity_ = new_capacity;
	}
}

uint64_t Segment::capacity() const
{
	lock_guard<recursive_mutex> lock(mutex_);
	return data_.size();
}

void Segment::append_data(void *data, uint64_t samples)
{
	lock_guard<recursive_mutex> lock(mutex_);

	assert(capacity_ >= sample_count_);

	// Ensure there's enough capacity to copy.
	const uint64_t free_space = capacity_ - sample_count_;
	if (free_space < samples)
		set_capacity(sample_count_ + samples);

	memcpy((uint8_t*)data_.data() + sample_count_ * unit_size_,
		data, samples * unit_size_);
	sample_count_ += samples;
}

} // namespace data
} // namespace pv
