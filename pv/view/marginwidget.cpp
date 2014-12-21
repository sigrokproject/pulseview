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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <QMenu>
#include <QMouseEvent>

#include "view.hpp"

#include "marginwidget.hpp"

#include <pv/widgets/popup.hpp>

using std::shared_ptr;

namespace pv {
namespace view {

MarginWidget::MarginWidget(View &parent) :
	QWidget(&parent),
	view_(parent),
	dragging_(false)
{
	setAttribute(Qt::WA_NoSystemBackground, true);
	setFocusPolicy(Qt::ClickFocus);
	setMouseTracking(true);
}

void MarginWidget::show_popup(const shared_ptr<ViewItem> &item)
{
	pv::widgets::Popup *const p = item->create_popup(this);
	if (p)
		p->show();
}

void MarginWidget::leaveEvent(QEvent*)
{
	mouse_point_ = QPoint(-1, -1);
	update();
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

void MarginWidget::clear_selection()
{
}

} // namespace view
} // namespace pv
