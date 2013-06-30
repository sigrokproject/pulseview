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

#include "view.h"
#include "viewport.h"

#include "signal.h"
#include "../sigsession.h"

#include <QMouseEvent>

#include <boost/foreach.hpp>

using namespace boost;
using namespace std;

namespace pv {
namespace view {

Viewport::Viewport(View &parent) :
	QWidget(&parent),
        _view(parent)
{
	setMouseTracking(true);
	setAutoFillBackground(true);
	setBackgroundRole(QPalette::Base);

	connect(&_view, SIGNAL(signals_moved()),
		this, SLOT(on_signals_moved()));
}

int Viewport::get_total_height() const
{
	int h = 0;
	const vector< shared_ptr<Trace> > traces(_view.get_traces());
	BOOST_FOREACH(const shared_ptr<Trace> t, traces) {
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
	const int v_offset = _view.v_offset();
	BOOST_FOREACH(const shared_ptr<Trace> t, traces)
	{
		assert(t);
		t->paint(p, t->get_v_offset() - v_offset, 0, width(),
			_view.scale(), _view.offset());
	}

	if (_view.cursors_shown())
		_view.cursors().draw_viewport_foreground(p, rect());

	p.end();
}

void Viewport::mousePressEvent(QMouseEvent *event)
{
	assert(event);

	_mouse_down_point = event->pos();
	_mouse_down_offset = _view.offset();
}

void Viewport::mouseMoveEvent(QMouseEvent *event)
{
	assert(event);

	if (event->buttons() & Qt::LeftButton)
	{
		_view.set_scale_offset(_view.scale(),
			_mouse_down_offset +
			(_mouse_down_point - event->pos()).x() *
			_view.scale());
	}
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

void Viewport::on_signals_moved()
{
	update();
}

} // namespace view
} // namespace pv
