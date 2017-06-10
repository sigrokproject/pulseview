/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2013 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#include <QMenu>
#include <QMouseEvent>

#include "view.hpp"

#include "marginwidget.hpp"

#include <pv/widgets/popup.hpp>

using std::shared_ptr;

namespace pv {
namespace views {
namespace trace {

MarginWidget::MarginWidget(View &parent) :
	ViewWidget(parent)
{
	setAttribute(Qt::WA_NoSystemBackground, true);
}

void MarginWidget::item_clicked(const shared_ptr<ViewItem> &item)
{
	if (item && item->enabled())
		show_popup(item);
}

void MarginWidget::show_popup(const shared_ptr<ViewItem> &item)
{
	pv::widgets::Popup *const p = item->create_popup(this);
	if (p)
		p->show();
}

void MarginWidget::contextMenuEvent(QContextMenuEvent *event)
{
	const shared_ptr<ViewItem> r = get_mouse_over_item(mouse_point_);
	if (!r)
		return;

	QMenu *menu = r->create_context_menu(this);
	if (menu)
		menu->exec(event->globalPos());
}

void MarginWidget::keyPressEvent(QKeyEvent *event)
{
	assert(event);

	if (event->key() == Qt::Key_Delete) {
		const auto items = this->items();
		for (auto &i : items)
			if (i->selected())
				i->delete_pressed();
	}
}

} // namespace trace
} // namespace views
} // namespace pv
