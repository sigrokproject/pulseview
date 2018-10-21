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

using std::find;
using std::make_pair;
using std::max;
using std::min;
using std::pair;
using std::shared_ptr;
using std::static_pointer_cast;
using std::vector;

namespace pv {
namespace views {
namespace trace {

const ViewItemOwner::item_list& TraceTreeItemOwner::child_items() const
{
	return items_;
}

vector< shared_ptr<TraceTreeItem> >
TraceTreeItemOwner::trace_tree_child_items() const
{
	vector< shared_ptr<TraceTreeItem> > items;
	for (auto &i : items_) {
		assert(dynamic_pointer_cast<TraceTreeItem>(i));
		const shared_ptr<TraceTreeItem> t(
			static_pointer_cast<TraceTreeItem>(i));
		items.push_back(t);
	}

	return items;
}

void TraceTreeItemOwner::clear_child_items()
{
	for (auto &t : trace_tree_child_items()) {
		assert(t->owner() == this);
		t->set_owner(nullptr);
	}
	items_.clear();
}

void TraceTreeItemOwner::add_child_item(shared_ptr<TraceTreeItem> item)
{
	assert(!item->owner());
	item->set_owner(this);
	items_.push_back(item);

	extents_changed(true, true);
}

void TraceTreeItemOwner::remove_child_item(shared_ptr<TraceTreeItem> item)
{
	assert(item->owner() == this);
	item->set_owner(nullptr);
	auto iter = find(items_.begin(), items_.end(), item);
	assert(iter != items_.end());
	items_.erase(iter);

	extents_changed(true, true);
}

pair<int, int> TraceTreeItemOwner::v_extents() const
{
	bool has_children = false;

	pair<int, int> extents(INT_MAX, INT_MIN);
	for (const shared_ptr<TraceTreeItem>& t : trace_tree_child_items()) {
		assert(t);
		if (!t->enabled())
			continue;

		has_children = true;

		const int child_offset = t->layout_v_offset();
		const pair<int, int> child_extents = t->v_extents();
		extents.first = min(child_extents.first + child_offset,
			extents.first);
		extents.second = max(child_extents.second + child_offset,
			extents.second);
	}

	if (!has_children)
		extents = make_pair(0, 0);

	return extents;
}

void TraceTreeItemOwner::restack_items()
{
	vector<shared_ptr<TraceTreeItem>> items(trace_tree_child_items());

	// Sort by the centre line of the extents
	stable_sort(items.begin(), items.end(),
		[](const shared_ptr<TraceTreeItem> &a, const shared_ptr<TraceTreeItem> &b) {
			const auto aext = a->v_extents();
			const auto bext = b->v_extents();
			return a->layout_v_offset() +
					(aext.first + aext.second) / 2 <
				b->layout_v_offset() +
					(bext.first + bext.second) / 2;
		});

	int total_offset = 0;
	for (shared_ptr<TraceTreeItem> r : items) {
		const pair<int, int> extents = r->v_extents();
		if (extents.first == 0 && extents.second == 0)
			continue;

		// We position disabled traces, so that they are close to the
		// animation target positon should they be re-enabled
		if (r->enabled())
			total_offset += -extents.first;

		if (!r->dragging())
			r->set_layout_v_offset(total_offset);

		if (r->enabled())
			total_offset += extents.second;
	}
}

} // namespace trace
} // namespace views
} // namespace pv
