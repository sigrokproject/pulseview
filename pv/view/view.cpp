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

#include <assert.h>
#include <limits.h>
#include <math.h>

#include <boost/foreach.hpp>

#include <QEvent>
#include <QMouseEvent>
#include <QScrollBar>

#include "header.h"
#include "ruler.h"
#include "signal.h"
#include "view.h"
#include "viewport.h"

#include "pv/sigsession.h"
#include "pv/data/logic.h"
#include "pv/data/logicsnapshot.h"

using namespace boost;
using namespace std;

namespace pv {
namespace view {

const double View::MaxScale = 1e9;
const double View::MinScale = 1e-15;

const int View::LabelMarginWidth = 70;
const int View::RulerHeight = 30;

const int View::MaxScrollValue = INT_MAX / 2;

const int View::SignalHeight = 30;
const int View::SignalMargin = 10;
const int View::SignalSnapGridSize = 10;

const QColor View::CursorAreaColour(220, 231, 243);

const QSizeF View::LabelPadding(4, 0);

View::View(SigSession &session, QWidget *parent) :
	QAbstractScrollArea(parent),
	_session(session),
	_viewport(new Viewport(*this)),
	_ruler(new Ruler(*this)),
	_header(new Header(*this)),
	_data_length(0),
	_scale(1e-6),
	_offset(0),
	_v_offset(0),
	_updating_scroll(false),
	_show_cursors(false),
	_cursors(*this),
	_hover_point(-1, -1)
{
	connect(horizontalScrollBar(), SIGNAL(valueChanged(int)),
		this, SLOT(h_scroll_value_changed(int)));
	connect(verticalScrollBar(), SIGNAL(valueChanged(int)),
		this, SLOT(v_scroll_value_changed(int)));

	connect(&_session, SIGNAL(signals_changed()),
		this, SLOT(signals_changed()));
	connect(&_session, SIGNAL(data_updated()),
		this, SLOT(data_updated()));

	connect(&_cursors.first(), SIGNAL(time_changed()),
		this, SLOT(marker_time_changed()));
	connect(&_cursors.second(), SIGNAL(time_changed()),
		this, SLOT(marker_time_changed()));

	connect(_header, SIGNAL(signals_moved()),
		this, SLOT(on_signals_moved()));

	connect(_header, SIGNAL(selection_changed()),
		_ruler, SLOT(clear_selection()));
	connect(_ruler, SIGNAL(selection_changed()),
		_header, SLOT(clear_selection()));

	setViewportMargins(LabelMarginWidth, RulerHeight, 0, 0);
	setViewport(_viewport);

	_viewport->installEventFilter(this);
	_ruler->installEventFilter(this);
	_header->installEventFilter(this);
}

SigSession& View::session()
{
	return _session;
}

double View::scale() const
{
	return _scale;
}

double View::offset() const
{
	return _offset;
}

int View::v_offset() const
{
	return _v_offset;
}

void View::zoom(double steps)
{
	zoom(steps, (width() - LabelMarginWidth) / 2);
}

void View::zoom(double steps, int offset)
{
	const double cursor_offset = _offset + _scale * offset;
	_scale *= pow(3.0/2.0, -steps);
	_scale = max(min(_scale, MaxScale), MinScale);
	_offset = cursor_offset - _scale * offset;

	_ruler->update();
	_viewport->update();
	update_scroll();
}


void View::set_scale_offset(double scale, double offset)
{
	_scale = scale;
	_offset = offset;

	update_scroll();
	_ruler->update();
	_viewport->update();
}

bool View::cursors_shown() const
{
	return _show_cursors;
}

void View::show_cursors(bool show)
{
	_show_cursors = show;
	_ruler->update();
	_viewport->update();
}

void View::centre_cursors()
{
	const double time_width = _scale * _viewport->width();
	_cursors.first().set_time(_offset + time_width * 0.4);
	_cursors.second().set_time(_offset + time_width * 0.6);
	_ruler->update();
	_viewport->update();
}

CursorPair& View::cursors()
{
	return _cursors;
}

const QPoint& View::hover_point() const
{
	return _hover_point;
}

void View::normalize_layout()
{
	const vector< shared_ptr<Signal> > sigs(_session.get_signals());

	int v_min = INT_MAX;
	BOOST_FOREACH(const shared_ptr<Signal> s, sigs)
		v_min = min(s->get_v_offset(), v_min);

	const int delta = -min(v_min, 0);
	BOOST_FOREACH(shared_ptr<Signal> s, sigs)
		s->set_v_offset(s->get_v_offset() + delta);

	verticalScrollBar()->setSliderPosition(_v_offset + delta);
	v_scroll_value_changed(verticalScrollBar()->sliderPosition());
}

void View::get_scroll_layout(double &length, double &offset) const
{
	const shared_ptr<data::SignalData> sig_data = _session.get_data();
	if (!sig_data)
		return;

	length = _data_length / (sig_data->get_samplerate() * _scale);
	offset = _offset / _scale;
}

void View::update_scroll()
{
	assert(_viewport);

	const QSize areaSize = _viewport->size();

	// Set the horizontal scroll bar
	double length = 0, offset = 0;
	get_scroll_layout(length, offset);
	length = max(length - areaSize.width(), 0.0);

	horizontalScrollBar()->setPageStep(areaSize.width() / 2);

	_updating_scroll = true;

	if (length < MaxScrollValue) {
		horizontalScrollBar()->setRange(0, length);
		horizontalScrollBar()->setSliderPosition(offset);
	} else {
		horizontalScrollBar()->setRange(0, MaxScrollValue);
		horizontalScrollBar()->setSliderPosition(
			_offset * MaxScrollValue / (_scale * length));
	}

	_updating_scroll = false;

	// Set the vertical scrollbar
	verticalScrollBar()->setPageStep(areaSize.height());
	verticalScrollBar()->setRange(0,
		_viewport->get_total_height() + SignalMargin -
		areaSize.height());
}

void View::reset_signal_layout()
{
	int offset = SignalMargin + SignalHeight;
	const vector< shared_ptr<Signal> > sigs(_session.get_signals());
	BOOST_FOREACH(shared_ptr<Signal> s, sigs) {
		s->set_v_offset(offset);
		offset += SignalHeight + 2 * SignalMargin;
	}

	normalize_layout();
}

bool View::eventFilter(QObject *object, QEvent *event)
{
	const QEvent::Type type = event->type();
	if (type == QEvent::MouseMove) {

		const QMouseEvent *const mouse_event = (QMouseEvent*)event;
		if (object == _viewport)
			_hover_point = mouse_event->pos();
		else if (object == _ruler)
			_hover_point = QPoint(mouse_event->x(), 0);
		else if (object == _header)
			_hover_point = QPoint(0, mouse_event->y());
		else
			_hover_point = QPoint(-1, -1);

		hover_point_changed();

	} else if (type == QEvent::Leave) {
		_hover_point = QPoint(-1, -1);
		hover_point_changed();
	}

	return QObject::eventFilter(object, event);
}

bool View::viewportEvent(QEvent *e)
{
	switch(e->type()) {
	case QEvent::Paint:
	case QEvent::MouseButtonPress:
	case QEvent::MouseButtonRelease:
	case QEvent::MouseButtonDblClick:
	case QEvent::MouseMove:
	case QEvent::Wheel:
		return false;

	default:
		return QAbstractScrollArea::viewportEvent(e);
	}
}

void View::resizeEvent(QResizeEvent*)
{
	_ruler->setGeometry(_viewport->x(), 0,
		_viewport->width(), _viewport->y());
	_header->setGeometry(0, _viewport->y(),
		_viewport->x(), _viewport->height());
	update_scroll();
}

void View::h_scroll_value_changed(int value)
{
	if (_updating_scroll)
		return;

	const int range = horizontalScrollBar()->maximum();
	if (range < MaxScrollValue)
		_offset = _scale * value;
	else {
		double length = 0, offset;
		get_scroll_layout(length, offset);
		_offset = _scale * length * value / MaxScrollValue;
	}

	_ruler->update();
	_viewport->update();
}

void View::v_scroll_value_changed(int value)
{
	_v_offset = value;
	_header->update();
	_viewport->update();
}

void View::signals_changed()
{
	reset_signal_layout();
}

void View::data_updated()
{
	// Get the new data length
	_data_length = 0;
	shared_ptr<data::Logic> sig_data = _session.get_data();
	if (sig_data) {
		deque< shared_ptr<data::LogicSnapshot> > &snapshots =
			sig_data->get_snapshots();
		BOOST_FOREACH(shared_ptr<data::LogicSnapshot> s, snapshots)
			if (s)
				_data_length = max(_data_length,
					s->get_sample_count());
	}

	// Update the scroll bars
	update_scroll();

	// Repaint the view
	_viewport->update();
}

void View::marker_time_changed()
{
	_ruler->update();
	_viewport->update();
}

void View::on_signals_moved()
{
	update_scroll();
	signals_moved();
}

} // namespace view
} // namespace pv
