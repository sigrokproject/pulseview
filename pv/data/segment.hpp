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

#ifndef PULSEVIEW_PV_DATA_SEGMENT_HPP
#define PULSEVIEW_PV_DATA_SEGMENT_HPP

#include "pv/util.hpp"

#include <thread>
#include <mutex>
#include <vector>

namespace pv {
namespace data {

class Segment
{
public:
	Segment(uint64_t samplerate, unsigned int unit_size);

	virtual ~Segment();

	uint64_t get_sample_count() const;

	const pv::util::Timestamp& start_time() const;

	double samplerate() const;
	void set_samplerate(double samplerate);

	unsigned int unit_size() const;

	/**
	 * @brief Increase the capacity of the segment.
	 *
	 * Increasing the capacity allows samples to be appended without needing
	 * to reallocate memory.
	 *
	 * For the best efficiency @c set_capacity() should be called once before
	 * @c append_data() is called to set up the segment with the expected number
	 * of samples that will be appended in total.
	 *
	 * @note The capacity will automatically be increased when @c append_data()
	 * is called if there is not enough capacity in the buffer to store the samples.
	 *
	 * @param[in] new_capacity The new capacity of the segment. If this value is
	 * 	smaller or equal than the current capacity then the method has no effect.
	 */
	void set_capacity(uint64_t new_capacity);

	/**
	 * @brief Get the current capacity of the segment.
	 *
	 * The capacity can be increased by calling @c set_capacity().
	 *
	 * @return The current capacity of the segment.
	 */
	uint64_t capacity() const;

protected:
	void append_data(void *data, uint64_t samples);

protected:
	mutable std::recursive_mutex mutex_;
	std::vector<uint8_t> data_;
	uint64_t sample_count_;
	pv::util::Timestamp start_time_;
	double samplerate_;
	uint64_t capacity_;
	unsigned int unit_size_;
};

} // namespace data
} // namespace pv

#endif // PULSEVIEW_PV_DATA_SEGMENT_HPP
