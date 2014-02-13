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

#ifndef PULSEVIEW_PV_DATA_SNAPSHOT_H
#define PULSEVIEW_PV_DATA_SNAPSHOT_H

#include <libsigrok/libsigrok.h>

#include <boost/thread.hpp>

namespace pv {
namespace data {

class Snapshot
{
public:
	Snapshot(int unit_size);

	virtual ~Snapshot();

	uint64_t get_sample_count() const;

	int unit_size() const;

	/**
	 * @brief Increase the capacity of the snapshot.
	 *
	 * Increasing the capacity allows samples to be appended without needing
	 * to reallocate memory.
	 *
	 * For the best efficiency @c set_capacity() should be called once before
	 * @c append_data() is called to set up the snapshot with the expected number
	 * of samples that will be appended in total.
	 *
	 * @note The capacity will automatically be increased when @c append_data()
	 * is called if there is not enough capacity in the buffer to store the samples.
	 *
	 * @param[in] new_capacity The new capacity of the snapshot. If this value is
	 * 	smaller or equal than the current capacity then the method has no effect.
	 */
	void set_capacity(uint64_t new_capacity);

	/**
	 * @brief Get the current capacity of the snapshot.
	 *
	 * The capacity can be increased by calling @c set_capacity().
	 *
	 * @return The current capacity of the snapshot.
	 */
	uint64_t capacity() const;

protected:
	void append_data(void *data, uint64_t samples);

protected:
	mutable boost::recursive_mutex _mutex;
	void *_data;
	uint64_t _sample_count;
	uint64_t _capacity;
	int _unit_size;
};

} // namespace data
} // namespace pv

#endif // PULSEVIEW_PV_DATA_SNAPSHOT_H
