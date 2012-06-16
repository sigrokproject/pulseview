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

#include "sigview.h"

#include "sigsession.h"
#include "signal.h"

#include <QMouseEvent>

#include <boost/foreach.hpp>

using namespace boost;
using namespace std;

const int SigView::SignalHeight = 50;

SigView::SigView(SigSession &session, QWidget *parent) :
	QGLWidget(parent),
        _session(session),
	_scale(1000000000ULL),
	_offset(0)
{
	connect(&_session, SIGNAL(dataUpdated()),
		this, SLOT(dataUpdated()));

	setMouseTracking(true);
}

void SigView::initializeGL()
{
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_COLOR_MATERIAL);
	glEnable(GL_BLEND);
	glEnable(GL_POLYGON_SMOOTH);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glClearColor(1.0, 1.0, 1.0, 0);
}

void SigView::resizeGL(int width, int height)
{
	glViewport(0, 0, (GLint)width, (GLint)height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, width, height, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);
}

void SigView::paintGL()
{
	glClear(GL_COLOR_BUFFER_BIT);

	QRect rect(0, 0, width(), SignalHeight);
	const vector< shared_ptr<Signal> > &sigs =
		_session.get_signals();
	BOOST_FOREACH(const shared_ptr<Signal> s, sigs)
	{
		assert(s);
		s->paint(*this, rect, _scale, _offset);
		rect.translate(0, SignalHeight);
	}
}

void SigView::dataUpdated()
{
	update();
}

void SigView::mouseMoveEvent(QMouseEvent *event)
{
	assert(event);
}

void SigView::mousePressEvent(QMouseEvent *event)
{
	assert(event);
}

void SigView::mouseReleaseEvent(QMouseEvent *event)
{
	assert(event);

	switch(event->button())
	{
	case Qt::LeftButton:
		_scale = (_scale * 2) / 3;
		break;

	case Qt::RightButton:
		_scale = (_scale * 3) / 2;
		break;
	}

	updateGL();
}

