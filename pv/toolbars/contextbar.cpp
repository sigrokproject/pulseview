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

#include <stdio.h>

#include <boost/shared_ptr.hpp>
#include <boost/foreach.hpp>

#include "contextbar.h"

#include <pv/view/selectableitem.h>

using namespace boost;
using namespace std;

namespace pv {
namespace toolbars {

ContextBar::ContextBar(QWidget *parent) :
	QToolBar(tr("Context Bar"), parent)
{
}

void ContextBar::set_selected_items(const list<
	weak_ptr<pv::view::SelectableItem> > &items)
{
	clear();

	if (items.empty())
		return;

	if (shared_ptr<pv::view::SelectableItem> item =
		items.front().lock()) {

		assert(item);

		const list<QAction*> actions(
			item->get_context_bar_actions());
		BOOST_FOREACH(QAction *action, actions) {
			assert(action);
			addAction(action);
		}
	}
}

} // namespace toolbars
} // namespace pv
