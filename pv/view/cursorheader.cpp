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
	dragging_(false),
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
	CursorPair &cursors = view_.cursors();
	cursors.first()->select(false);
	cursors.second()->select(false);
	update();
}

void CursorHeader::paintEvent(QPaintEvent*)
{
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);

	// Draw the cursors
	if (view_.cursors_shown()) {
		// The cursor labels are not drawn with the arrows exactly on the
		// bottom line of the widget, because then the selection shadow
		// would be clipped away.
		const QRect r = rect().adjusted(0, 0, 0, -BaselineOffset);
		view_.cursors().draw_markers(p, r);
	}
}

void CursorHeader::mouseMoveEvent(QMouseEvent *e)
{
	if (!(e->buttons() & Qt::LeftButton))
		return;

	if ((e->pos() - mouse_down_point_).manhattanLength() <
		QApplication::startDragDistance())
		return;

	dragging_ = true;

	if (shared_ptr<TimeMarker> m = grabbed_marker_.lock())
		m->set_time(view_.offset() +
			((double)e->x() + 0.5) * view_.scale());
}

void CursorHeader::mousePressEvent(QMouseEvent *e)
{
	if (e->buttons() & Qt::LeftButton) {
		mouse_down_point_ = e->pos();

		grabbed_marker_.reset();

		clear_selection();

		if (view_.cursors_shown()) {
			CursorPair &cursors = view_.cursors();
			if (cursors.first()->get_label_rect(
				rect()).contains(e->pos()))
				grabbed_marker_ = cursors.first();
			else if (cursors.second()->get_label_rect(
				rect()).contains(e->pos()))
				grabbed_marker_ = cursors.second();
		}

		if (shared_ptr<TimeMarker> m = grabbed_marker_.lock())
			m->select();

		selection_changed();
	}
}

void CursorHeader::mouseReleaseEvent(QMouseEvent *)
{
	using pv::widgets::Popup;

	if (!dragging_)
		if (shared_ptr<TimeMarker> m = grabbed_marker_.lock()) {
			Popup *const p = m->create_popup(&view_);
			const QPoint arrpos(m->get_x(), height() - BaselineOffset);
			p->set_position(mapToGlobal(arrpos), Popup::Bottom);
			p->show();
		}

	dragging_ = false;
	grabbed_marker_.reset();
}

} // namespace view
} // namespace pv
