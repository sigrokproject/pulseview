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

#include "extdef.h"

#include <QMouseEvent>

#include <math.h>

#include <boost/foreach.hpp>

using namespace boost;
using namespace std;

const int SigView::SignalHeight = 50;
const int SigView::LabelMarginWidth = 70;
const int SigView::RulerHeight = 30;

const int SigView::ScaleUnits[3] = {1, 2, 5};

SigView::SigView(SigSession &session, QWidget *parent) :
	QGLWidget(parent),
        _session(session),
	_scale(1e-6),
	_offset(0)
{
	connect(&_session, SIGNAL(dataUpdated()),
		this, SLOT(dataUpdated()));

	setMouseTracking(true);
	setAutoFillBackground(false);
}

void SigView::initializeGL()
{
}

void SigView::resizeGL(int width, int height)
{
	setupViewport(width, height);
}

void SigView::paintEvent(QPaintEvent *event)
{
	int offset;

	const vector< shared_ptr<Signal> > &sigs =
		_session.get_signals();

	// Prepare for OpenGL rendering
	makeCurrent();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();

	setupViewport(width(), height());

	qglClearColor(Qt::white);
	glClear(GL_COLOR_BUFFER_BIT);

	// Plot the signal
	offset = RulerHeight;
	BOOST_FOREACH(const shared_ptr<Signal> s, sigs)
	{
		assert(s);

		const QRect signal_rect(LabelMarginWidth, offset,
			width() - LabelMarginWidth, SignalHeight);

		s->paint(*this, signal_rect, _scale, _offset);

		offset += SignalHeight;
	}

	// Prepare for QPainter rendering
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	// Paint the labels
	offset = RulerHeight;
	BOOST_FOREACH(const shared_ptr<Signal> s, sigs)
	{
		assert(s);

		const QRect label_rect(0, offset,
			LabelMarginWidth, SignalHeight);
		s->paint_label(painter, label_rect);

		offset += SignalHeight;
	}

	// Paint the ruler
	paintRuler(painter);

	painter.end();
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

	const double cursor_offset = _offset + _scale * (double)event->x();

	switch(event->button())
	{
	case Qt::LeftButton:
		_scale *= 2.0 / 3.0;
		break;

	case Qt::RightButton:
		_scale *= 3.0 / 2.0;
		break;
	}

	_offset = cursor_offset - _scale * (double)event->x();

	update();
}

void SigView::setupViewport(int width, int height)
{
	glViewport(0, 0, (GLint)width, (GLint)height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, width, height, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);
}

void SigView::paintRuler(QPainter &p)
{
	const double MinSpacing = 20;

	double tick_period = 0.0f;
	const double min_period = _scale * MinSpacing;

	double order = 10e-15;
	while(tick_period < min_period)
	{
		int unit = 0;
		while(tick_period < min_period &&
			unit < countof(ScaleUnits))
			tick_period = order * ScaleUnits[unit++];
		order *= 10;
	}

	const double tick_seperation = tick_period / _scale;

	p.setPen(Qt::transparent);
	p.setBrush(QColor(0xC0, 0xC0, 0xC0));
	p.drawRect(LabelMarginWidth, 0,
		width() - LabelMarginWidth, RulerHeight);

	p.setPen(Qt::black);

	const double offset_ticks = -_offset / tick_period;
	double x = (offset_ticks - floor(offset_ticks)) *
		tick_seperation + LabelMarginWidth;
	while(x < width())
	{
		p.drawLine(x, 0, x, RulerHeight);
		x += tick_seperation;
	}
}
