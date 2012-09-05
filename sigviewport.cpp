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

#include "sigviewport.h"

#include "sigsession.h"
#include "signal.h"
#include "sigview.h"

#include "extdef.h"

#include <QMouseEvent>
#include <QTextStream>

#include <math.h>

#include <boost/foreach.hpp>

using namespace boost;
using namespace std;

const int SigViewport::SignalHeight = 50;

const int SigViewport::MinorTickSubdivision = 4;
const int SigViewport::ScaleUnits[3] = {1, 2, 5};

const QString SigViewport::SIPrefixes[9] =
	{"f", "p", "n", QChar(0x03BC), "m", "", "k", "M", "G"};
const int SigViewport::FirstSIPrefixPower = -15;

SigViewport::SigViewport(SigView &parent) :
	QGLWidget(&parent),
        _view(parent)
{
	setMouseTracking(true);
	setAutoFillBackground(false);
}

int SigViewport::get_total_height() const
{
	int height = 0;
	BOOST_FOREACH(const shared_ptr<Signal> s,
		_view._session.get_signals()) {
		assert(s);
		height += SignalHeight;
	}

	return height;
}

void SigViewport::initializeGL()
{
}

void SigViewport::resizeGL(int width, int height)
{
	setup_viewport(width, height);
}

void SigViewport::paintEvent(QPaintEvent *event)
{
	int offset;

	const vector< shared_ptr<Signal> > &sigs =
		_view._session.get_signals();

	// Prepare for OpenGL rendering
	makeCurrent();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();

	setup_viewport(width(), height());

	qglClearColor(Qt::white);
	glClear(GL_COLOR_BUFFER_BIT);

	// Plot the signal
	glEnable(GL_SCISSOR_TEST);
	glScissor(SigView::LabelMarginWidth, 0, width(), height());
	offset = SigView::RulerHeight - _view.v_offset();
	BOOST_FOREACH(const shared_ptr<Signal> s, sigs)
	{
		assert(s);

		const QRect signal_rect(SigView::LabelMarginWidth, offset,
			width() - SigView::LabelMarginWidth, SignalHeight);

		s->paint(*this, signal_rect, _view.scale(), _view.offset());

		offset += SignalHeight;
	}

	glDisable(GL_SCISSOR_TEST);

	// Prepare for QPainter rendering
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	// Paint the labels
	offset = SigView::RulerHeight - _view.v_offset();
	BOOST_FOREACH(const shared_ptr<Signal> s, sigs)
	{
		assert(s);

		const QRect label_rect(0, offset,
			SigView::LabelMarginWidth, SignalHeight);
		s->paint_label(painter, label_rect);

		offset += SignalHeight;
	}

	// Paint the ruler
	paint_ruler(painter);

	painter.end();
}

void SigViewport::mousePressEvent(QMouseEvent *event)
{
	assert(event);

	_mouse_down_point = event->pos();
	_mouse_down_offset = _view.offset();
}

void SigViewport::mouseMoveEvent(QMouseEvent *event)
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

void SigViewport::mouseReleaseEvent(QMouseEvent *event)
{
	assert(event);
}

void SigViewport::wheelEvent(QWheelEvent *event)
{
	assert(event);
	_view.zoom(event->delta() / 120, event->x() -
		SigView::LabelMarginWidth);
}

void SigViewport::setup_viewport(int width, int height)
{
	glViewport(0, 0, (GLint)width, (GLint)height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, width, height, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);
}

void SigViewport::paint_ruler(QPainter &p)
{
	const double MinSpacing = 80;

	const double min_period = _view.scale() * MinSpacing;

	const int order = (int)floorf(log10f(min_period));
	const double order_decimal = pow(10, order);

	int unit = 0;
	double tick_period = 0.0f;

	do
	{
		tick_period = order_decimal * ScaleUnits[unit++];
	} while(tick_period < min_period && unit < countof(ScaleUnits));

	const int prefix = (order - FirstSIPrefixPower) / 3;
	assert(prefix >= 0);
	assert(prefix < countof(SIPrefixes));

	const int text_height = p.boundingRect(0, 0, INT_MAX, INT_MAX,
		Qt::AlignLeft | Qt::AlignTop, "8").height();

	// Draw the tick marks
	p.setPen(Qt::black);

	const double minor_tick_period = tick_period / MinorTickSubdivision;
	const double first_major_division =
		floor(_view.offset() / tick_period);
	const double first_minor_division =
		ceil(_view.offset() / minor_tick_period);
	const double t0 = first_major_division * tick_period;

	int division = (int)round(first_minor_division -
		first_major_division * MinorTickSubdivision);
	while(1)
	{
		const double t = t0 + division * minor_tick_period;
		const double x = (t - _view.offset()) / _view.scale() +
			SigView::LabelMarginWidth;

		if(x >= width())
			break;

		if(division % MinorTickSubdivision == 0)
		{
			// Draw a major tick
			QString s;
			QTextStream ts(&s);
			ts << (t / order_decimal) << SIPrefixes[prefix] << "s";
			p.drawText(x, 0, 0, text_height, Qt::AlignCenter | Qt::AlignTop |
				Qt::TextDontClip, s);
			p.drawLine(x, text_height, x, SigView::RulerHeight);
		}
		else
		{
			// Draw a minor tick
			p.drawLine(x,
				(text_height + SigView::RulerHeight) / 2, x,
				SigView::RulerHeight);
		}

		division++;
	}
}
