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

#include <QApplication>
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

void MarginWidget::mouse_left_press_event(QMouseEvent *event)
{
	(void)event;

	const bool ctrl_pressed =
		QApplication::keyboardModifiers() & Qt::ControlModifier;

	// Clear selection if control is not pressed and this item is unselected
	if ((!mouse_down_item_ || !mouse_down_item_->selected()) &&
		!ctrl_pressed)
		clear_selection();

	// Set the signal selection state if the item has been clicked
	if (mouse_down_item_) {
		if (ctrl_pressed)
			mouse_down_item_->select(!mouse_down_item_->selected());
		else
			mouse_down_item_->select(true);
	}

	// Save the offsets of any signals which will be dragged
	const auto items = this->items();
	for (auto &i : items)
		if (i->selected())
			i->drag();

	selection_changed();
	update();
}

void MarginWidget::mouse_left_release_event(QMouseEvent *event)
{
	assert(event);

	auto items = this->items();
	const bool ctrl_pressed =
		QApplication::keyboardModifiers() & Qt::ControlModifier;

	// Unselect everything if control is not pressed
	const shared_ptr<ViewItem> mouse_over =
		get_mouse_over_item(event->pos());

	for (auto &i : items)
		i->drag_release();

	if (dragging_)
		view_.restack_all_row_items();
	else
	{
		if (!ctrl_pressed) {
			for (shared_ptr<ViewItem> i : items)
				if (mouse_down_item_ != i)
					i->select(false);

			if (mouse_down_item_)
				show_popup(mouse_down_item_);
		}
	}

	dragging_ = false;
}

void MarginWidget::mousePressEvent(QMouseEvent *event)
{
	assert(event);

	mouse_down_point_ = event->pos();
	mouse_down_item_ = get_mouse_over_item(event->pos());

	if (event->button() & Qt::LeftButton)
		mouse_left_press_event(event);
}

void MarginWidget::mouseReleaseEvent(QMouseEvent *event)
{
	assert(event);
	if (event->button() & Qt::LeftButton)
		mouse_left_release_event(event);

	mouse_down_item_ = nullptr;
}

void MarginWidget::mouseMoveEvent(QMouseEvent *event)
{
	assert(event);
	mouse_point_ = event->pos();

	if (!(event->buttons() & Qt::LeftButton))
		return;

	if ((event->pos() - mouse_down_point_).manhattanLength() <
		QApplication::startDragDistance())
		return;

	if (!accept_drag())
		return;

	// Do the drag
	dragging_ = true;
	drag_items(event->pos() - mouse_down_point_);

	update();
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

void MarginWidget::keyPressEvent(QKeyEvent *e)
{
	assert(e);

	if (e->key() == Qt::Key_Delete)
	{
		const auto items = this->items();
		for (auto &i : items)
			if (i->selected())
				i->delete_pressed();
	}
}

void MarginWidget::clear_selection()
{
	const auto items = this->items();
	for (auto &i : items)
		i->select(false);
	update();
}

} // namespace view
} // namespace pv
