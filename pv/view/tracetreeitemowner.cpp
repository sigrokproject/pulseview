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

vector< shared_ptr<TraceTreeItem> >& TraceTreeItemOwner::child_items()
{
	return items_;
}

const vector< shared_ptr<TraceTreeItem> >& TraceTreeItemOwner::child_items() const
{
	return items_;
}

void TraceTreeItemOwner::clear_child_items()
{
	for (auto &i : items_) {
		assert(i->owner() == this);
		i->set_owner(nullptr);
	}
	items_.clear();
}

void TraceTreeItemOwner::add_child_item(std::shared_ptr<TraceTreeItem> item)
{
	assert(!item->owner());
	item->set_owner(this);
	items_.push_back(item);

	extents_changed(true, true);
}

void TraceTreeItemOwner::remove_child_item(std::shared_ptr<TraceTreeItem> item)
{
	assert(item->owner() == this);
	item->set_owner(nullptr);
	auto iter = std::find(items_.begin(), items_.end(), item);
	assert(iter != items_.end());
	items_.erase(iter);

	extents_changed(true, true);
}

TraceTreeItemOwner::iterator TraceTreeItemOwner::begin()
{
	return iterator(this, items_.begin());
}

TraceTreeItemOwner::iterator TraceTreeItemOwner::end()
{
	return iterator(this);
}

TraceTreeItemOwner::const_iterator TraceTreeItemOwner::begin() const
{
	return const_iterator(this, items_.cbegin());
}

TraceTreeItemOwner::const_iterator TraceTreeItemOwner::end() const
{
	return const_iterator(this);
}

pair<int, int> TraceTreeItemOwner::v_extents() const
{
	pair<int, int> extents(INT_MAX, INT_MIN);

	for (const shared_ptr<TraceTreeItem> r : child_items()) {
		assert(r);
		if (!r->enabled())
			continue;

		const int child_offset = r->layout_v_offset();
		const pair<int, int> child_extents = r->v_extents();
		extents.first = min(child_extents.first + child_offset,
			extents.first);
		extents.second = max(child_extents.second + child_offset,
			extents.second);
	}

	return extents;
}

void TraceTreeItemOwner::restack_items()
{
}

} // view
} // pv
