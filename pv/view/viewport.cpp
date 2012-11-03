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
}

int Viewport::get_total_height() const
{
	int height = 0;
	BOOST_FOREACH(const shared_ptr<Signal> s,
		_view.session().get_signals()) {
		assert(s);
		height += View::SignalHeight;
	}

	return height;
}

void Viewport::paintEvent(QPaintEvent *event)
{
	const vector< shared_ptr<Signal> > &sigs =
		_view.session().get_signals();

	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);

	draw_cursors_background(p);

	// Plot the signal
	int offset = -_view.v_offset();
	BOOST_FOREACH(const shared_ptr<Signal> s, sigs)
	{
		assert(s);

		const QRect signal_rect(0, offset,
			width(), View::SignalHeight);

		s->paint(p, signal_rect, _view.scale(), _view.offset());

		offset += View::SignalHeight;
	}

	draw_cursors_foreground(p);

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

	if(event->buttons() & Qt::LeftButton)
	{
		_view.set_scale_offset(_view.scale(),
			_mouse_down_offset +
			(_mouse_down_point - event->pos()).x() *
			_view.scale());
	}
}

void Viewport::mouseReleaseEvent(QMouseEvent *event)
{
	assert(event);
}

void Viewport::wheelEvent(QWheelEvent *event)
{
	assert(event);
	_view.zoom(event->delta() / 120, event->x());
}

void Viewport::draw_cursors_background(QPainter &p)
{
	if(!_view.cursors_shown())
		return;

	p.setPen(Qt::NoPen);
	p.setBrush(QBrush(View::CursorAreaColour));

	const pair<Cursor, Cursor> &c = _view.cursors();
	const float x1 = (c.first.time() - _view.offset()) / _view.scale();
	const float x2 = (c.second.time() - _view.offset()) / _view.scale();
	const int l = (int)max(min(x1, x2), 0.0f);
	const int r = (int)min(max(x1, x2), (float)width());

	p.drawRect(l, 0, r - l, height());
}

void Viewport::draw_cursors_foreground(QPainter &p)
{
	if(!_view.cursors_shown())
		return;

	const QRect r = rect();
	pair<Cursor, Cursor> &cursors = _view.cursors();
	cursors.first.paint(p, r);
	cursors.second.paint(p, r);
}

} // namespace view
} // namespace pv
