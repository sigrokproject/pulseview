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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <cassert>

#include "tracetreeitem.hpp"
#include "trace.hpp"
#include "tracetreeitemowner.hpp"

namespace pv {
namespace views {
namespace trace {

const ViewItemOwner::item_list& ViewItemOwner::child_items() const
{
	return items_;
}

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

} // namespace trace
} // namespace views
} // namespace pv
