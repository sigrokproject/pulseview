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

#include <algorithm>

#include "rowitem.hpp"
#include "timeitem.hpp"
#include "view.hpp"
#include "viewwidget.hpp"

using std::any_of;
using std::shared_ptr;
using std::vector;

namespace pv {
namespace view {

ViewWidget::ViewWidget(View &parent) :
	QWidget(&parent),
	view_(parent)
{
}

bool ViewWidget::accept_drag() const
{
	const vector< shared_ptr<TimeItem> > items(view_.time_items());

	const bool any_row_items_selected = any_of(view_.begin(), view_.end(),
		[](const shared_ptr<RowItem> &r) { return r->selected(); });

	const bool any_time_items_selected = any_of(items.begin(), items.end(),
		[](const shared_ptr<TimeItem> &i) { return i->selected(); });

	if (any_row_items_selected && !any_time_items_selected)
	{
		// Check all the drag items share a common owner
		RowItemOwner *item_owner = nullptr;
		for (shared_ptr<RowItem> r : view_)
			if (r->dragging()) {
				if (!item_owner)
					item_owner = r->owner();
				else if(item_owner != r->owner())
					return false;
			}

		return true;
	}
	else if (any_time_items_selected && !any_row_items_selected)
	{
		return true;
	}

	return false;
}

} // namespace view
} // namespace pv
