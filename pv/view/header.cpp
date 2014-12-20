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

#include "header.hpp"
#include "view.hpp"

#include "signal.hpp"
#include "tracegroup.hpp"

#include <cassert>
#include <algorithm>

#include <boost/iterator/filter_iterator.hpp>

#include <QApplication>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QRect>

#include <pv/session.hpp>
#include <pv/widgets/popup.hpp>

using boost::make_filter_iterator;
using std::dynamic_pointer_cast;
using std::max;
using std::make_pair;
using std::min;
using std::pair;
using std::shared_ptr;
using std::stable_sort;
using std::vector;

namespace pv {
namespace view {

const int Header::Padding = 12;
const int Header::BaselineOffset = 5;

static bool item_selected(shared_ptr<RowItem> r)
{
	return r->selected();
}

Header::Header(View &parent) :
	MarginWidget(parent)
{
	connect(&view_, SIGNAL(signals_moved()),
		this, SLOT(on_signals_moved()));
}

QSize Header::sizeHint() const
{
	QRectF max_rect(-Padding, 0, Padding, 0);
	for (auto &i : view_)
		if (i->enabled())
			max_rect = max_rect.united(i->label_rect(QRect()));
	return QSize(max_rect.width() + Padding + BaselineOffset, 0);
}

QSize Header::extended_size_hint() const
{
	return sizeHint() + QSize(ViewItem::HighlightRadius, 0);
}

shared_ptr<RowItem> Header::get_mouse_over_item(const QPoint &pt)
{
	const QRect r(0, 0, width() - BaselineOffset, height());
	for (auto &i : view_)
		if (i->enabled() && i->label_rect(r).contains(pt))
			return i;
	return shared_ptr<RowItem>();
}

void Header::clear_selection()
{
	for (auto &i : view_)
		i->select(false);
	update();
}

void Header::paintEvent(QPaintEvent*)
{
	// The trace labels are not drawn with the arrows exactly on the
	// left edge of the widget, because then the selection shadow
	// would be clipped away.
	const QRect rect(0, 0, width() - BaselineOffset, height());

	vector< shared_ptr<RowItem> > row_items(
		view_.begin(), view_.end());

	stable_sort(row_items.begin(), row_items.end(),
		[](const shared_ptr<RowItem> &a, const shared_ptr<RowItem> &b) {
			return a->visual_v_offset() < b->visual_v_offset(); });

	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	for (const shared_ptr<RowItem> r : row_items)
	{
		assert(r);

		const bool highlight = !dragging_ &&
			r->label_rect(rect).contains(mouse_point_);
		r->paint_label(painter, rect, highlight);
	}

	painter.end();
}

void Header::mouseLeftPressEvent(QMouseEvent *event)
{
	(void)event;

	const bool ctrl_pressed =
		QApplication::keyboardModifiers() & Qt::ControlModifier;

	// Clear selection if control is not pressed and this item is unselected
	if ((!mouse_down_item_ || !mouse_down_item_->selected()) &&
		!ctrl_pressed)
		for (shared_ptr<RowItem> r : view_)
			r->select(false);

	// Set the signal selection state if the item has been clicked
	if (mouse_down_item_) {
		if (ctrl_pressed)
			mouse_down_item_->select(!mouse_down_item_->selected());
		else
			mouse_down_item_->select(true);
	}

	// Save the offsets of any signals which will be dragged
	for (const shared_ptr<RowItem> r : view_)
		if (r->selected())
			r->drag();

	selection_changed();
	update();
}

void Header::mousePressEvent(QMouseEvent *event)
{
	assert(event);

	mouse_down_point_ = event->pos();
	mouse_down_item_ = get_mouse_over_item(event->pos());

	if (event->button() & Qt::LeftButton)
		mouseLeftPressEvent(event);
}

void Header::mouseLeftReleaseEvent(QMouseEvent *event)
{
	assert(event);

	const bool ctrl_pressed =
		QApplication::keyboardModifiers() & Qt::ControlModifier;

	// Unselect everything if control is not pressed
	const shared_ptr<RowItem> mouse_over =
		get_mouse_over_item(event->pos());

	for (auto &r : view_)
		r->drag_release();

	if (dragging_)
		view_.restack_all_row_items();
	else
	{
		if (!ctrl_pressed) {
			for (shared_ptr<RowItem> r : view_)
				if (mouse_down_item_ != r)
					r->select(false);

			if (mouse_down_item_)
				show_popup(mouse_down_item_);
		}
	}

	dragging_ = false;
}

void Header::mouseReleaseEvent(QMouseEvent *event)
{
	assert(event);
	if (event->button() & Qt::LeftButton)
		mouseLeftReleaseEvent(event);

	mouse_down_item_ = nullptr;
}

void Header::mouseMoveEvent(QMouseEvent *event)
{
	assert(event);
	mouse_point_ = event->pos();

	if (!(event->buttons() & Qt::LeftButton))
		return;

	if ((event->pos() - mouse_down_point_).manhattanLength() <
		QApplication::startDragDistance())
		return;

	// Check all the drag items share a common owner
	RowItemOwner *item_owner = nullptr;
	for (shared_ptr<RowItem> r : view_)
		if (r->dragging()) {
			if (!item_owner)
				item_owner = r->owner();
			else if(item_owner != r->owner())
				return;
		}

	if (!item_owner)
		return;

	// Do the drag
	dragging_ = true;

	const QPoint delta = event->pos() - mouse_down_point_;

	for (std::shared_ptr<RowItem> r : view_)
		if (r->dragging()) {
			r->drag_by(delta);

			// Ensure the trace is selected
			r->select();
		}

	item_owner->restack_items();
	for (const auto &r : *item_owner)
		r->animate_to_layout_v_offset();
	signals_moved();

	update();
}

void Header::leaveEvent(QEvent*)
{
	mouse_point_ = QPoint(-1, -1);
	update();
}

void Header::contextMenuEvent(QContextMenuEvent *event)
{
	const shared_ptr<RowItem> r = get_mouse_over_item(mouse_point_);
	if (!r)
		return;

	QMenu *menu = r->create_context_menu(this);
	if (!menu)
		menu = new QMenu(this);

	if (std::count_if(view_.begin(), view_.end(), item_selected) > 1)
	{
		menu->addSeparator();

		QAction *const group = new QAction(tr("Group"), this);
		QList<QKeySequence> shortcuts;
		shortcuts.append(QKeySequence(Qt::ControlModifier | Qt::Key_G));
		group->setShortcuts(shortcuts);
		connect(group, SIGNAL(triggered()), this, SLOT(on_group()));
		menu->addAction(group);
	}

	menu->exec(event->globalPos());
}

void Header::keyPressEvent(QKeyEvent *e)
{
	assert(e);

	if (e->key() == Qt::Key_Delete)
	{
		for (const shared_ptr<RowItem> r : view_)
			if (r->selected())
				r->delete_pressed();
	}
	else if (e->key() == Qt::Key_G && e->modifiers() == Qt::ControlModifier)
		on_group();
	else if (e->key() == Qt::Key_U && e->modifiers() == Qt::ControlModifier)
		on_ungroup();
}

void Header::on_signals_moved()
{
	update();
}

void Header::on_group()
{
	vector< shared_ptr<RowItem> > selected_items(
		make_filter_iterator(item_selected, view_.begin(), view_.end()),
		make_filter_iterator(item_selected, view_.end(), view_.end()));
	stable_sort(selected_items.begin(), selected_items.end(),
		[](const shared_ptr<RowItem> &a, const shared_ptr<RowItem> &b) {
			return a->visual_v_offset() < b->visual_v_offset(); });

	shared_ptr<TraceGroup> group(new TraceGroup());
	shared_ptr<RowItem> focus_item(
		mouse_down_item_ ? mouse_down_item_ : selected_items.front());

	assert(focus_item);
	assert(focus_item->owner());
	focus_item->owner()->add_child_item(group);

	// Set the group v_offset here before reparenting
	group->force_to_v_offset(focus_item->layout_v_offset() +
		focus_item->v_extents().first);

	for (size_t i = 0; i < selected_items.size(); i++) {
		const shared_ptr<RowItem> &r = selected_items[i];
		assert(r->owner());
		r->owner()->remove_child_item(r);
		group->add_child_item(r);

		// Put the items at 1-pixel offsets, so that restack will
		// stack them in the right order
		r->set_layout_v_offset(i);
	}
}

void Header::on_ungroup()
{
	bool restart;
	do {
		restart = false;
		for (const shared_ptr<RowItem> r : view_) {
			const shared_ptr<TraceGroup> tg =
				dynamic_pointer_cast<TraceGroup>(r);
			if (tg && tg->selected()) {
				tg->ungroup();
				restart = true;
				break;
			}
		}
	} while(restart);
}

} // namespace view
} // namespace pv
