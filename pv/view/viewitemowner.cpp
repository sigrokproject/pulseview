/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2014 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#include "tracetreeitem.hpp"
#include "tracetreeitemowner.hpp"
#include "trace.hpp"

using std::dynamic_pointer_cast;
using std::max;
using std::make_pair;
using std::min;
using std::pair;
using std::set;
using std::shared_ptr;
using std::vector;

namespace pv {
namespace view {

ViewItemOwner::iterator ViewItemOwner::begin()
{
	return iterator(this, items_.begin());
}

ViewItemOwner::iterator ViewItemOwner::end()
{
	return iterator(this);
}

ViewItemOwner::const_iterator ViewItemOwner::begin() const
{
	return const_iterator(this, items_.cbegin());
}

ViewItemOwner::const_iterator ViewItemOwner::end() const
{
	return const_iterator(this);
}

} // view
} // pv
