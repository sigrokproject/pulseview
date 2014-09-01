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

#ifndef PULSEVIEW_PV_VIEW_ROWITEMOWNER_H
#define PULSEVIEW_PV_VIEW_ROWITEMOWNER_H

#include <memory>
#include <vector>

namespace pv {

class SigSession;

namespace view {

class RowItem;
class View;

class RowItemOwner
{
public:
	/**
	 * Returns the session of the onwer.
	 */
	virtual pv::SigSession& session() = 0;

	/**
	 * Returns the session of the owner.
	 */
	virtual const pv::SigSession& session() const = 0;

	/**
	 * Returns the view of the owner.
	 */
	virtual pv::view::View* view() = 0;

	/**
	 * Returns the view of the owner.
	 */
	virtual const pv::view::View* view() const = 0;

	virtual int owner_v_offset() const = 0;

	/**
	 * Returns a list of row items owned by this object.
	 */
	virtual const std::vector< std::shared_ptr<RowItem> >&
		child_items() const;

	/**
	 * Clears the list of child items.
	 */
	void clear_child_items();

	/**
	 * Adds a child item to this object.
	 */
	void add_child_item(std::shared_ptr<RowItem> item);

	/**
	 * Removes a child item from this object.
	 */
	void remove_child_item(std::shared_ptr<RowItem> item);

	virtual void update_viewport() = 0;

private:
	std::vector< std::shared_ptr<RowItem> > _items;
};

} // view
} // pv

#endif // PULSEVIEW_PV_VIEW_ROWITEMOWNER_H
