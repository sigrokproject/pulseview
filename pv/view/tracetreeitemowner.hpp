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

#ifndef PULSEVIEW_PV_VIEW_TRACETREEITEMOWNER_HPP
#define PULSEVIEW_PV_VIEW_TRACETREEITEMOWNER_HPP

#include <memory>
#include <vector>

#include "rowitemiterator.hpp"

namespace pv {

class Session;

namespace view {

class TraceTreeItem;
class View;

class TraceTreeItemOwner
{
public:
	typedef std::vector< std::shared_ptr<TraceTreeItem> > item_list;
	typedef RowItemIterator<TraceTreeItemOwner, TraceTreeItem> iterator;
	typedef RowItemIterator<const TraceTreeItemOwner, TraceTreeItem> const_iterator;

public:
	/**
	 * Returns the session of the onwer.
	 */
	virtual pv::Session& session() = 0;

	/**
	 * Returns the session of the owner.
	 */
	virtual const pv::Session& session() const = 0;

	/**
	 * Returns the view of the owner.
	 */
	virtual pv::view::View* view() = 0;

	/**
	 * Returns the view of the owner.
	 */
	virtual const pv::view::View* view() const = 0;

	virtual int owner_visual_v_offset() const = 0;

	/**
	 * Returns the number of nested parents that this row item owner has.
	 */
	virtual unsigned int depth() const = 0;

	/**
	 * Returns a list of row items owned by this object.
	 */
	virtual item_list& child_items();

	/**
	 * Returns a list of row items owned by this object.
	 */
	virtual const item_list& child_items() const;

	/**
	 * Clears the list of child items.
	 */
	void clear_child_items();

	/**
	 * Adds a child item to this object.
	 */
	void add_child_item(std::shared_ptr<TraceTreeItem> item);

	/**
	 * Removes a child item from this object.
	 */
	void remove_child_item(std::shared_ptr<TraceTreeItem> item);

	/**
	 * Returns a depth-first iterator at the beginning of the child TraceTreeItem
	 * tree.
	 */
	iterator begin();

	/**
	 * Returns a depth-first iterator at the end of the child TraceTreeItem tree.
	 */
	iterator end();

	/**
	 * Returns a constant depth-first iterator at the beginning of the
	 * child TraceTreeItem tree.
	 */
	const_iterator begin() const;

	/**
	 * Returns a constant depth-first iterator at the end of the child
	 * TraceTreeItem tree.
	 */
	const_iterator end() const;

	/**
	 * Makes a list of row item owners of all the row items that are
	 * decendants of this item.
	 */
	std::set< TraceTreeItemOwner* > list_row_item_owners();

	/**
	 * Creates a list of decendant signals filtered by type.
	 */
	template<class T>
	std::set< std::shared_ptr<T> > list_by_type();

	/**
	 * Computes the vertical extents of the contents of this row item owner.
	 * @return A pair containing the minimum and maximum y-values.
	 */
	std::pair<int, int> v_extents() const;

	virtual void restack_items();

public:
	virtual void row_item_appearance_changed(bool label, bool content) = 0;

	virtual void extents_changed(bool horz, bool vert) = 0;

private:
	item_list items_;
};

} // view
} // pv

#endif // PULSEVIEW_PV_VIEW_TRACETREEITEMOWNER_HPP
