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

#include <boost/foreach.hpp>

#include "analog.h"
#include "analogsnapshot.h"

using namespace boost;
using namespace std;

namespace pv {
namespace data {

Analog::Analog() :
	SignalData()
{
}

void Analog::push_snapshot(shared_ptr<AnalogSnapshot> &snapshot)
{
	_snapshots.push_front(snapshot);
}

deque< shared_ptr<AnalogSnapshot> >& Analog::get_snapshots()
{
	return _snapshots;
}

void Analog::clear()
{
	_snapshots.clear();
}

uint64_t Analog::get_max_sample_count() const
{
	uint64_t l = 0;
	BOOST_FOREACH(const boost::shared_ptr<AnalogSnapshot> s, _snapshots) {
		assert(s);
		l = max(l, s->get_sample_count());
	}
	return l;
}

} // namespace data
} // namespace pv
