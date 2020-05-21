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

#ifndef PULSEVIEW_PV_VIEWS_TRACE_TRACETREEITEMOWNER_HPP
#define PULSEVIEW_PV_VIEWS_TRACE_TRACETREEITEMOWNER_HPP

#include "viewitemowner.hpp"
#include "tracetreeitem.hpp"

using std::pair;
using std::shared_ptr;
using std::vector;

namespace pv {

class Session;

namespace views {
namespace trace {

class TraceTreeItem;
class View;

class TraceTreeItemOwner : public ViewItemOwner
{
public:
	/**
	 * Returns the view of the owner.
	 */
	virtual View* view() = 0;

	/**
	 * Returns the view of the owner.
	 */
	virtual const View* view() const = 0;

	virtual int owner_visual_v_offset() const = 0;

	/**
	 * Returns the session of the owner.
	 */
	virtual Session& session() = 0;

	/**
	 * Returns the session of the owner.
	 */
	virtual const Session& session() const = 0;

	/**
	 * Returns the number of nested parents that this row item owner has.
	 */
	virtual unsigned int depth() const = 0;

	/**
	 * Returns a list of row items owned by this object.
	 */
	virtual const item_list& child_items() const;

	/**
	 * Returns a list of row items owned by this object.
	 */
	vector< shared_ptr<TraceTreeItem> > trace_tree_child_items() const;

	/**
	 * Clears the list of child items.
	 */
	void clear_child_items();

	/**
	 * Adds a child item to this object.
	 */
	void add_child_item(shared_ptr<TraceTreeItem> item);

	/**
	 * Removes a child item from this object.
	 */
	void remove_child_item(shared_ptr<TraceTreeItem> item);

	virtual void restack_items();

	/**
	 * Computes the vertical extents of the contents of this row item owner.
	 * @return A pair containing the minimum and maximum y-values.
	 */
	pair<int, int> v_extents() const;

public:
	virtual void row_item_appearance_changed(bool label, bool content) = 0;

	virtual void extents_changed(bool horz, bool vert) = 0;
};

} // namespace trace
} // namespace views
} // namespace pv

#endif // PULSEVIEW_PV_VIEWS_TRACE_TRACETREEITEMOWNER_HPP
