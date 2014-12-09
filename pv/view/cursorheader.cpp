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

#include "cursorheader.hpp"

#include "ruler.hpp"
#include "view.hpp"

#include <QApplication>
#include <QFontMetrics>
#include <QMouseEvent>

#include <pv/widgets/popup.hpp>

using std::shared_ptr;
using std::vector;

namespace pv {
namespace view {

const int CursorHeader::Padding = 20;
const int CursorHeader::BaselineOffset = 5;

int CursorHeader::calculateTextHeight()
{
	QFontMetrics fm(font());
	return fm.boundingRect(0, 0, INT_MAX, INT_MAX,
		Qt::AlignLeft | Qt::AlignTop, "8").height();
}

CursorHeader::CursorHeader(View &parent) :
	MarginWidget(parent),
	textHeight_(calculateTextHeight())
{
	setMouseTracking(true);
}

QSize CursorHeader::sizeHint() const
{
	return QSize(0, textHeight_ + Padding + BaselineOffset);
}

void CursorHeader::clear_selection()
{
	const vector< shared_ptr<TimeItem> > items(view_.time_items());
	for (auto &i : items)
		i->select(false);
	update();
}

void CursorHeader::paintEvent(QPaintEvent*)
{
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);

	// The cursor labels are not drawn with the arrows exactly on the
	// bottom line of the widget, because then the selection shadow
	// would be clipped away.
	const QRect r = rect().adjusted(0, 0, 0, -BaselineOffset);

	// Draw the items
	const vector< shared_ptr<TimeItem> > items(view_.time_items());
	for (auto &m : items)
		m->paint_label(p, r);
}

void CursorHeader::mouseMoveEvent(QMouseEvent *e)
{
	mouse_point_ = e->pos();

	if (!(e->buttons() & Qt::LeftButton))
		return;

	if ((e->pos() - mouse_down_point_).manhattanLength() <
		QApplication::startDragDistance())
		return;

	// Do the drag
	dragging_ = true;

	const int delta = e->pos().x() - mouse_down_point_.x();
	const vector< shared_ptr<TimeItem> > items(view_.time_items());
	for (auto &i : items)
		if (i->dragging())
			i->set_time(view_.offset() +
				(i->drag_point().x() + delta) * view_.scale());
}

void CursorHeader::mousePressEvent(QMouseEvent *e)
{
	if (e->buttons() & Qt::LeftButton) {
		mouse_down_point_ = e->pos();

		mouse_down_item_.reset();

		clear_selection();

		const vector< shared_ptr<TimeItem> > items(view_.time_items());
		for (auto &i : items)
			if (i && i->label_rect(rect()).contains(e->pos())) {
				mouse_down_item_ = i;
				break;
			}

		if (mouse_down_item_) {
			mouse_down_item_->select();
			mouse_down_item_->drag();
		}

		selection_changed();
	}
}

void CursorHeader::mouseReleaseEvent(QMouseEvent *)
{
	using pv::widgets::Popup;

	if (!dragging_ && mouse_down_item_) {
		Popup *const p = mouse_down_item_->create_popup(&view_);
		if (p) {
			const QPoint arrpos(mouse_down_item_->get_x(),
				height() - BaselineOffset);
			p->set_position(mapToGlobal(arrpos), Popup::Bottom);
			p->show();
		}
	}

	dragging_ = false;
	mouse_down_item_.reset();

	const vector< shared_ptr<TimeItem> > items(view_.time_items());
	for (auto &i : items)
		i->drag_release();
}

void CursorHeader::leaveEvent(QEvent*)
{
	mouse_point_ = QPoint(-1, -1);
	update();
}

} // namespace view
} // namespace pv
