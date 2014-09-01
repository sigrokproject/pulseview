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

#ifdef ENABLE_DECODE
#include <libsigrokdecode/libsigrokdecode.h>
#endif

#include <assert.h>
#include <limits.h>
#include <math.h>

#include <QEvent>
#include <QMouseEvent>
#include <QScrollBar>

#include "cursorheader.h"
#include "decodetrace.h"
#include "header.h"
#include "ruler.h"
#include "signal.h"
#include "view.h"
#include "viewport.h"

#include "pv/sigsession.h"
#include "pv/data/logic.h"
#include "pv/data/logicsnapshot.h"

using pv::data::SignalData;
using std::back_inserter;
using std::deque;
using std::list;
using std::max;
using std::make_pair;
using std::min;
using std::pair;
using std::set;
using std::shared_ptr;
using std::vector;
using std::weak_ptr;

namespace pv {
namespace view {

const double View::MaxScale = 1e9;
const double View::MinScale = 1e-15;

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
	_cursorheader(new CursorHeader(*this)),
	_header(new Header(*this)),
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
	connect(&_session, SIGNAL(capture_state_changed(int)),
		this, SLOT(data_updated()));
	connect(&_session, SIGNAL(data_received()),
		this, SLOT(data_updated()));
	connect(&_session, SIGNAL(frame_ended()),
		this, SLOT(data_updated()));

	connect(_cursors.first().get(), SIGNAL(time_changed()),
		this, SLOT(marker_time_changed()));
	connect(_cursors.second().get(), SIGNAL(time_changed()),
		this, SLOT(marker_time_changed()));

	connect(_header, SIGNAL(geometry_updated()),
		this, SLOT(on_geometry_updated()));
	connect(_header, SIGNAL(signals_moved()),
		this, SLOT(on_signals_moved()));

	connect(_header, SIGNAL(selection_changed()),
		_cursorheader, SLOT(clear_selection()));
	connect(_cursorheader, SIGNAL(selection_changed()),
		_header, SLOT(clear_selection()));

	connect(_header, SIGNAL(selection_changed()),
		this, SIGNAL(selection_changed()));
	connect(_cursorheader, SIGNAL(selection_changed()),
		this, SIGNAL(selection_changed()));

	connect(this, SIGNAL(hover_point_changed()),
		this, SLOT(on_hover_point_changed()));

	setViewport(_viewport);

	_viewport->installEventFilter(this);
	_ruler->installEventFilter(this);
	_cursorheader->installEventFilter(this);
	_header->installEventFilter(this);

	// Trigger the initial event manually. The default device has signals
	// which were created before this object came into being
	signals_changed();

	// make sure the transparent widgets are on the top
	_cursorheader->raise();
	_header->raise();
}

SigSession& View::session()
{
	return _session;
}

const SigSession& View::session() const
{
	return _session;
}

Viewport* View::viewport()
{
	return _viewport;
}

const Viewport* View::viewport() const
{
	return _viewport;
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
	zoom(steps, _viewport->width() / 2);
}

void View::zoom(double steps, int offset)
{
	set_zoom(_scale * pow(3.0/2.0, -steps), offset);
}

void View::zoom_fit()
{
	const pair<double, double> extents = get_time_extents();
	const double delta = extents.second - extents.first;
	if (delta < 1e-12)
		return;

	assert(_viewport);
	const int w = _viewport->width();
	if (w <= 0)
		return;

	const double scale = max(min(delta / w, MaxScale), MinScale);
	set_scale_offset(scale, extents.first);
}

void View::zoom_one_to_one()
{
	using pv::data::SignalData;

	const vector< shared_ptr<Signal> > sigs(
		session().get_signals());

	// Make a set of all the visible data objects
	set< shared_ptr<SignalData> > visible_data = get_visible_data();
	if (visible_data.empty())
		return;

	double samplerate = 0.0;
	for (const shared_ptr<SignalData> d : visible_data) {
		assert(d);
		samplerate = max(samplerate, d->samplerate());
	}

	if (samplerate == 0.0)
		return;

	assert(_viewport);
	const int w = _viewport->width();
	if (w <= 0)
		return;

	set_zoom(1.0 / samplerate, w / 2);
}

void View::set_scale_offset(double scale, double offset)
{
	_scale = scale;
	_offset = offset;

	update_scroll();
	_ruler->update();
	_cursorheader->update();
	_viewport->update();
	scale_offset_changed();
}

vector< shared_ptr<Trace> > View::get_traces() const
{
	vector< shared_ptr<Trace> > traces;

	const vector< shared_ptr<Signal> > sigs(
		session().get_signals());
	copy(sigs.begin(), sigs.end(), back_inserter(traces));

#ifdef ENABLE_DECODE
	const vector< shared_ptr<DecodeTrace> > decode_sigs(
		session().get_decode_signals());
	copy(decode_sigs.begin(), decode_sigs.end(), back_inserter(traces));
#endif

	stable_sort(traces.begin(), traces.end(),
		[](const shared_ptr<Trace> &a, const shared_ptr<Trace> &b) {
			return a->get_v_offset() < b->get_v_offset(); });

	return traces;
}

list<weak_ptr<SelectableItem> > View::selected_items() const
{
	list<weak_ptr<SelectableItem> > items;

	// List the selected signals
	const vector< shared_ptr<Trace> > traces(get_traces());
	for (shared_ptr<Trace> t : traces) {
		assert(t);
		if (t->selected())
			items.push_back(t);
	}

	// List the selected cursors
	if (_cursors.first()->selected())
		items.push_back(_cursors.first());
	if (_cursors.second()->selected())
		items.push_back(_cursors.second());

	return items;
}

set< shared_ptr<SignalData> > View::get_visible_data() const
{
	const vector< shared_ptr<Signal> > sigs(
		session().get_signals());

	// Make a set of all the visible data objects
	set< shared_ptr<SignalData> > visible_data;
	for (const shared_ptr<Signal> sig : sigs)
		if (sig->enabled())
			visible_data.insert(sig->data());

	return visible_data;
}

pair<double, double> View::get_time_extents() const
{
	const set< shared_ptr<SignalData> > visible_data = get_visible_data();
	if (visible_data.empty())
		return make_pair(0.0, 0.0);

	double left_time = DBL_MAX, right_time = DBL_MIN;
	for (const shared_ptr<SignalData> d : visible_data)
	{
		const double start_time = d->get_start_time();
		double samplerate = d->samplerate();
		samplerate = (samplerate <= 0.0) ? 1.0 : samplerate;

		left_time = min(left_time, start_time);
		right_time = max(right_time, start_time +
			d->get_max_sample_count() / samplerate);
	}

	assert(left_time < right_time);
	return make_pair(left_time, right_time);
}

bool View::cursors_shown() const
{
	return _show_cursors;
}

void View::show_cursors(bool show)
{
	_show_cursors = show;
	_cursorheader->update();
	_viewport->update();
}

void View::centre_cursors()
{
	const double time_width = _scale * _viewport->width();
	_cursors.first()->set_time(_offset + time_width * 0.4);
	_cursors.second()->set_time(_offset + time_width * 0.6);
	_cursorheader->update();
	_viewport->update();
}

CursorPair& View::cursors()
{
	return _cursors;
}

const CursorPair& View::cursors() const
{
	return _cursors;
}

const QPoint& View::hover_point() const
{
	return _hover_point;
}

void View::normalize_layout()
{
	const vector< shared_ptr<Trace> > traces(get_traces());

	int v_min = INT_MAX;
	for (const shared_ptr<Trace> t : traces)
		v_min = min(t->get_v_offset(), v_min);

	const int delta = -min(v_min, 0);
	for (shared_ptr<Trace> t : traces)
		t->set_v_offset(t->get_v_offset() + delta);

	verticalScrollBar()->setSliderPosition(_v_offset + delta);
	v_scroll_value_changed(verticalScrollBar()->sliderPosition());
}

void View::update_viewport()
{
	assert(_viewport);
	_viewport->update();
}

void View::get_scroll_layout(double &length, double &offset) const
{
	const pair<double, double> extents = get_time_extents();
	length = (extents.second - extents.first) / _scale;
	offset = _offset / _scale;
}

void View::set_zoom(double scale, int offset)
{
	const double cursor_offset = _offset + _scale * offset;
	const double new_scale = max(min(scale, MaxScale), MinScale);
	const double new_offset = cursor_offset - new_scale * offset;
	set_scale_offset(new_scale, new_offset);
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

void View::update_layout()
{
	setViewportMargins(
		_header->sizeHint().width() - pv::view::Header::BaselineOffset,
		_ruler->sizeHint().height(), 0, 0);
	_ruler->setGeometry(_viewport->x(), 0,
		_viewport->width(), _viewport->y());
	_cursorheader->setGeometry(
		_viewport->x(),
		_ruler->sizeHint().height() - _cursorheader->sizeHint().height() / 2,
		_viewport->width(), _cursorheader->sizeHint().height());
	_header->setGeometry(0, _viewport->y(),
		_header->sizeHint().width(), _viewport->height());
	update_scroll();
}

bool View::eventFilter(QObject *object, QEvent *event)
{
	const QEvent::Type type = event->type();
	if (type == QEvent::MouseMove) {

		const QMouseEvent *const mouse_event = (QMouseEvent*)event;
		if (object == _viewport)
			_hover_point = mouse_event->pos();
		else if (object == _ruler || object == _cursorheader)
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
	case QEvent::TouchBegin:
	case QEvent::TouchUpdate:
	case QEvent::TouchEnd:
		return false;

	default:
		return QAbstractScrollArea::viewportEvent(e);
	}
}

void View::resizeEvent(QResizeEvent*)
{
	update_layout();
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
	_cursorheader->update();
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
	int offset = SignalMargin + SignalHeight;
	const vector< shared_ptr<Trace> > traces(get_traces());
	for (shared_ptr<Trace> t : traces) {
		t->set_view(this);
		t->set_v_offset(offset);
		offset += SignalHeight + 2 * SignalMargin;
	}

	update_layout();
	normalize_layout();
}

void View::data_updated()
{
	// Update the scroll bars
	update_scroll();

	// Repaint the view
	_viewport->update();
}

void View::marker_time_changed()
{
	_cursorheader->update();
	_viewport->update();
}

void View::on_signals_moved()
{
	update_scroll();
	signals_moved();
}

void View::on_geometry_updated()
{
	update_layout();
}

void View::on_hover_point_changed()
{
	const vector< shared_ptr<Trace> > traces(get_traces());
	for (shared_ptr<Trace> t : traces)
		t->hover_point_changed();
}

} // namespace view
} // namespace pv
