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

#include <boost/foreach.hpp>

#include "analogdatasnapshot.h"

using namespace boost;
using namespace std;

namespace pv {

AnalogDataSnapshot::AnalogDataSnapshot(
	const sr_datafeed_analog &analog) :
	DataSnapshot(sizeof(float))
{
	lock_guard<recursive_mutex> lock(_mutex);
	append_payload(analog);
}

void AnalogDataSnapshot::append_payload(
	const sr_datafeed_analog &analog)
{
	lock_guard<recursive_mutex> lock(_mutex);
	append_data(analog.data, analog.num_samples);
}

const float* AnalogDataSnapshot::get_samples() const
{
	return (const float*)_data;
}

} // namespace pv
