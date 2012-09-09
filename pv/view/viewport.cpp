/*
 * This file is part of the sigrok project.
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

#include "../../sigsession.h"
#include "../../signal.h"

#include <QMouseEvent>

#include <boost/foreach.hpp>

using namespace boost;
using namespace std;

namespace pv {
namespace view {

Viewport::Viewport(View &parent) :
	QGLWidget(&parent),
        _view(parent)
{
	setMouseTracking(true);
	setAutoFillBackground(false);
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

void Viewport::initializeGL()
{
}

void Viewport::resizeGL(int width, int height)
{
	setup_viewport(width, height);
}

void Viewport::paintEvent(QPaintEvent *event)
{
	int offset;

	const vector< shared_ptr<Signal> > &sigs =
		_view.session().get_signals();

	// Prepare for OpenGL rendering
	makeCurrent();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();

	setup_viewport(width(), height());

	qglClearColor(Qt::white);
	glClear(GL_COLOR_BUFFER_BIT);

	// Plot the signal
	glEnable(GL_SCISSOR_TEST);
	glScissor(0, 0, width(), height());
	offset = -_view.v_offset();
	BOOST_FOREACH(const shared_ptr<Signal> s, sigs)
	{
		assert(s);

		const QRect signal_rect(0, offset,
			width(), View::SignalHeight);

		s->paint(*this, signal_rect, _view.scale(), _view.offset());

		offset += View::SignalHeight;
	}

	glDisable(GL_SCISSOR_TEST);

	// Prepare for QPainter rendering
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	QPainter painter(this);
	painter.end();
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

void Viewport::setup_viewport(int width, int height)
{
	glViewport(0, 0, (GLint)width, (GLint)height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, width, height, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);
}

} // namespace view
} // namespace pv
