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

#include <cassert>

#include "logic.h"
#include "logicsnapshot.h"

using std::deque;
using std::max;
using std::shared_ptr;

namespace pv {
namespace data {

Logic::Logic(unsigned int num_channels) :
	SignalData(),
	_num_channels(num_channels)
{
	assert(_num_channels > 0);
}

int Logic::get_num_channels() const
{
	return _num_channels;
}

void Logic::push_snapshot(
	shared_ptr<LogicSnapshot> &snapshot)
{
	_snapshots.push_front(snapshot);
}

deque< shared_ptr<LogicSnapshot> >& Logic::get_snapshots()
{
	return _snapshots;
}

void Logic::clear()
{
	_snapshots.clear();
}

uint64_t Logic::get_max_sample_count() const
{
	uint64_t l = 0;
	for (std::shared_ptr<LogicSnapshot> s : _snapshots) {
		assert(s);
		l = max(l, s->get_sample_count());
	}
	return l;
}

} // namespace data
} // namespace pv
