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

#ifndef PULSEVIEW_PV_DATA_ANALOGSNAPSHOT_H
#define PULSEVIEW_PV_DATA_ANALOGSNAPSHOT_H

#include "snapshot.h"

#include <utility>
#include <vector>

namespace pv {
namespace data {

class AnalogSnapshot : public Snapshot
{
public:
	AnalogSnapshot(const sr_datafeed_analog &analog);

	void append_payload(const sr_datafeed_analog &analog);

	const float* get_samples() const;
};

} // namespace data
} // namespace pv

#endif // PULSEVIEW_PV_DATA_ANALOGSNAPSHOT_H
