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

#include <cassert>
#include <cmath>

#include "view.h"
#include "viewport.h"

#include "signal.h"
#include "../sigsession.h"

#include <QMouseEvent>

using std::abs;
using std::max;
using std::min;
using std::shared_ptr;
using std::vector;

namespace pv {
namespace view {

Viewport::Viewport(View &parent) :
	QWidget(&parent),
	_view(parent),
	_mouse_down_valid(false),
	_pinch_zoom_active(false)
{
	setAttribute(Qt::WA_AcceptTouchEvents, true);

	setMouseTracking(true);
	setAutoFillBackground(true);
	setBackgroundRole(QPalette::Base);

	connect(&_view.session(), SIGNAL(signals_changed()),
		this, SLOT(on_signals_changed()));

	connect(&_view, SIGNAL(signals_moved()),
		this, SLOT(on_signals_moved()));

	// Trigger the initial event manually. The default device has signals
	// which were created before this object came into being
	on_signals_changed();
}

int Viewport::get_total_height() const
{
	int h = 0;
	const vector< shared_ptr<Trace> > traces(_view.get_traces());
	for (const shared_ptr<Trace> t : traces) {
		assert(t);
		h = max(t->get_v_offset() + View::SignalHeight, h);
	}

	return h;
}

void Viewport::paintEvent(QPaintEvent*)
{
	const vector< shared_ptr<Trace> > traces(_view.get_traces());

	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);

	if (_view.cursors_shown())
		_view.cursors().draw_viewport_background(p, rect());

	// Plot the signal
	for (const shared_ptr<Trace> t : traces)
	{
		assert(t);
		t->paint_back(p, 0, width());
	}

	for (const shared_ptr<Trace> t : traces)
		t->paint_mid(p, 0, width());

	for (const shared_ptr<Trace> t : traces)
		t->paint_fore(p, 0, width());

	if (_view.cursors_shown())
		_view.cursors().draw_viewport_foreground(p, rect());

	p.end();
}

bool Viewport::event(QEvent *event)
{
	switch (event->type()) {
	case QEvent::TouchBegin:
	case QEvent::TouchUpdate:
	case QEvent::TouchEnd:
		if (touchEvent(static_cast<QTouchEvent *>(event)))
			return true;
		break;

	default:
		break;
	}

	return QWidget::event(event);
}

void Viewport::mousePressEvent(QMouseEvent *event)
{
	assert(event);

	if (event->button() == Qt::LeftButton) {
		_mouse_down_point = event->pos();
		_mouse_down_offset = _view.offset();
		_mouse_down_valid = true;
	}
}

void Viewport::mouseReleaseEvent(QMouseEvent *event)
{
	assert(event);

	if (event->button() == Qt::LeftButton)
		_mouse_down_valid = false;
}

void Viewport::mouseMoveEvent(QMouseEvent *event)
{
	assert(event);

	if (event->buttons() & Qt::LeftButton) {
		if (!_mouse_down_valid) {
			_mouse_down_point = event->pos();
			_mouse_down_offset = _view.offset();
			_mouse_down_valid = true;
		}

		_view.set_scale_offset(_view.scale(),
			_mouse_down_offset +
			(_mouse_down_point - event->pos()).x() *
			_view.scale());
	}
}

void Viewport::mouseDoubleClickEvent(QMouseEvent *event)
{
	assert(event);

	if (event->buttons() & Qt::LeftButton)
		_view.zoom(2.0, event->x());
	else if (event->buttons() & Qt::RightButton)
		_view.zoom(-2.0, event->x());
}

void Viewport::wheelEvent(QWheelEvent *event)
{
	assert(event);

	if (event->orientation() == Qt::Vertical) {
		// Vertical scrolling is interpreted as zooming in/out
		_view.zoom(event->delta() / 120, event->x());
	} else if (event->orientation() == Qt::Horizontal) {
		// Horizontal scrolling is interpreted as moving left/right
		_view.set_scale_offset(_view.scale(),
				       event->delta() * _view.scale()
				       + _view.offset());
	}
}

bool Viewport::touchEvent(QTouchEvent *event)
{
	QList<QTouchEvent::TouchPoint> touchPoints = event->touchPoints();

	if (touchPoints.count() != 2) {
		_pinch_zoom_active = false;
		return false;
	}

	const QTouchEvent::TouchPoint &touchPoint0 = touchPoints.first();
	const QTouchEvent::TouchPoint &touchPoint1 = touchPoints.last();

	if (!_pinch_zoom_active ||
	    (event->touchPointStates() & Qt::TouchPointPressed)) {
		_pinch_offset0 = _view.offset() + _view.scale() * touchPoint0.pos().x();
		_pinch_offset1 = _view.offset() + _view.scale() * touchPoint1.pos().x();
		_pinch_zoom_active = true;
	}

	double w = touchPoint1.pos().x() - touchPoint0.pos().x();
	if (abs(w) >= 1.0) {
		double scale = (_pinch_offset1 - _pinch_offset0) / w;
		if (scale < 0)
			scale = -scale;
		double offset = _pinch_offset0 - touchPoint0.pos().x() * scale;
		if (scale > 0)
			_view.set_scale_offset(scale, offset);
	}

	if (event->touchPointStates() & Qt::TouchPointReleased) {
		_pinch_zoom_active = false;

		if (touchPoint0.state() & Qt::TouchPointReleased) {
			// Primary touch released
			_mouse_down_valid = false;
		} else {
			// Update the mouse down fields so that continued
			// dragging with the primary touch will work correctly
			_mouse_down_point = touchPoint0.pos().toPoint();
			_mouse_down_offset = _view.offset();
			_mouse_down_valid = true;
		}
	}

	return true;
}

void Viewport::on_signals_changed()
{
	const vector< shared_ptr<Trace> > traces(_view.get_traces());
	for (shared_ptr<Trace> t : traces) {
		assert(t);
		connect(t.get(), SIGNAL(visibility_changed()),
			this, SLOT(update()));
	}
}

void Viewport::on_signals_moved()
{
	update();
}

} // namespace view
} // namespace pv
