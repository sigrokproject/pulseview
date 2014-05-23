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

#include "cursorheader.h"

#include "view.h"

#include <QApplication>
#include <QMouseEvent>

#include <pv/widgets/popup.h>

using std::shared_ptr;

namespace pv {
namespace view {

const int CursorHeader::CursorHeaderHeight = 26;

CursorHeader::CursorHeader(View &parent) :
	MarginWidget(parent),
	_dragging(false)
{
	setMouseTracking(true);
}

QSize CursorHeader::sizeHint() const
{
	return QSize(0, CursorHeaderHeight);
}

void CursorHeader::clear_selection()
{
	CursorPair &cursors = _view.cursors();
	cursors.first()->select(false);
	cursors.second()->select(false);
	update();
}

void CursorHeader::paintEvent(QPaintEvent*)
{
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);

	// Draw the cursors
	if (_view.cursors_shown()) {
		_view.cursors().draw_markers(p, rect(), 0); //prefix);
	}
}

void CursorHeader::mouseMoveEvent(QMouseEvent *e)
{
	if (!(e->buttons() & Qt::LeftButton))
		return;

	if ((e->pos() - _mouse_down_point).manhattanLength() <
		QApplication::startDragDistance())
		return;

	_dragging = true;

	if (shared_ptr<TimeMarker> m = _grabbed_marker.lock())
		m->set_time(_view.offset() +
			((double)e->x() + 0.5) * _view.scale());
}

void CursorHeader::mousePressEvent(QMouseEvent *e)
{
	if (e->buttons() & Qt::LeftButton) {
		_mouse_down_point = e->pos();

		_grabbed_marker.reset();

		clear_selection();

		if (_view.cursors_shown()) {
			CursorPair &cursors = _view.cursors();
			if (cursors.first()->get_label_rect(
				rect()).contains(e->pos()))
				_grabbed_marker = cursors.first();
			else if (cursors.second()->get_label_rect(
				rect()).contains(e->pos()))
				_grabbed_marker = cursors.second();
		}

		if (shared_ptr<TimeMarker> m = _grabbed_marker.lock())
			m->select();

		selection_changed();
	}
}

void CursorHeader::mouseReleaseEvent(QMouseEvent *)
{
	using pv::widgets::Popup;

	if (!_dragging)
		if (shared_ptr<TimeMarker> m = _grabbed_marker.lock()) {
			Popup *const p = m->create_popup(&_view);
			p->set_position(mapToGlobal(QPoint(m->get_x(),
				height())), Popup::Bottom);
			p->show();
		}

	_dragging = false;
	_grabbed_marker.reset();
}

} // namespace view
} // namespace pv
