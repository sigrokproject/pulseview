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

#include "header.h"
#include "view.h"

#include "signal.h"
#include "../sigsession.h"

#include <assert.h>

#include <boost/foreach.hpp>

#include <QApplication>
#include <QColorDialog>
#include <QInputDialog>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QRect>

using namespace boost;
using namespace std;

namespace pv {
namespace view {

Header::Header(View &parent) :
	MarginWidget(parent),
	_action_set_name(new QAction(tr("Set &Name..."), this)),
	_action_set_colour(new QAction(tr("Set &Colour..."), this))
{
	setMouseTracking(true);

	connect(_action_set_name, SIGNAL(triggered()),
		this, SLOT(on_action_set_name_triggered()));
	connect(_action_set_colour, SIGNAL(triggered()),
		this, SLOT(on_action_set_colour_triggered()));

	connect(&_view.session(), SIGNAL(signals_changed()),
		this, SLOT(on_signals_changed()));

	connect(&_view, SIGNAL(signals_moved()),
		this, SLOT(on_signals_moved()));
}

shared_ptr<Trace> Header::get_mouse_over_trace(const QPoint &pt)
{
	const int w = width();
	const vector< shared_ptr<Trace> > traces(_view.get_traces());

	const int v_offset = _view.v_offset();
	BOOST_FOREACH(const shared_ptr<Trace> t, traces)
	{
		assert(t);

		if (t->pt_in_label_rect(t->get_v_offset() - v_offset,
			0, w, pt))
			return t;
	}

	return shared_ptr<Trace>();
}

void Header::clear_selection()
{
	const vector< shared_ptr<Trace> > traces(_view.get_traces());
	BOOST_FOREACH(const shared_ptr<Trace> t, traces) {
		assert(t);
		t->select(false);
	}

	update();
}

void Header::paintEvent(QPaintEvent*)
{
	const int w = width();
	const vector< shared_ptr<Trace> > traces(_view.get_traces());

	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	const int v_offset = _view.v_offset();
	const bool dragging = !_drag_traces.empty();
	BOOST_FOREACH(const shared_ptr<Trace> t, traces)
	{
		assert(t);

		const int y = t->get_v_offset() - v_offset;
		const bool highlight = !dragging && t->pt_in_label_rect(
			y, 0, w, _mouse_point);
		t->paint_label(painter, y, w, highlight);
	}

	painter.end();
}

void Header::mousePressEvent(QMouseEvent *event)
{
	assert(event);

	const vector< shared_ptr<Trace> > traces(_view.get_traces());

	if (event->button() & Qt::LeftButton) {
		_mouse_down_point = event->pos();

		// Save the offsets of any signals which will be dragged
		BOOST_FOREACH(const shared_ptr<Trace> t, traces)
			if (t->selected())
				_drag_traces.push_back(
					make_pair(t, t->get_v_offset()));
	}

	// Select the signal if it has been clicked
	const shared_ptr<Trace> mouse_over_trace =
		get_mouse_over_trace(event->pos());
	if (mouse_over_trace) {
		if (mouse_over_trace->selected())
			mouse_over_trace->select(false);
		else {
			mouse_over_trace->select(true);

			if (~QApplication::keyboardModifiers() &
				Qt::ControlModifier)
				_drag_traces.clear();

			// Add the signal to the drag list
			if (event->button() & Qt::LeftButton)
				_drag_traces.push_back(
					make_pair(mouse_over_trace,
					mouse_over_trace->get_v_offset()));
		}
	}

	if (~QApplication::keyboardModifiers() & Qt::ControlModifier) {
		// Unselect all other signals because the Ctrl is not
		// pressed
		BOOST_FOREACH(const shared_ptr<Trace> t, traces)
			if (t != mouse_over_trace)
				t->select(false);
	}

	selection_changed();
	update();
}

void Header::mouseReleaseEvent(QMouseEvent *event)
{
	assert(event);
	if (event->button() == Qt::LeftButton) {
		_drag_traces.clear();
		_view.normalize_layout();
	}
}

void Header::mouseMoveEvent(QMouseEvent *event)
{
	assert(event);
	_mouse_point = event->pos();

	// Move the signals if we are dragging
	if (!_drag_traces.empty()) {
		const int delta = event->pos().y() - _mouse_down_point.y();

		for (std::list<std::pair<boost::weak_ptr<Trace>,
			int> >::iterator i = _drag_traces.begin();
			i != _drag_traces.end(); i++) {
			const boost::shared_ptr<Trace> trace((*i).first);
			if (trace) {
				const int y = (*i).second + delta;
				const int y_snap =
					((y + View::SignalSnapGridSize / 2) /
						View::SignalSnapGridSize) *
						View::SignalSnapGridSize;
				trace->set_v_offset(y_snap);

				// Ensure the trace is selected
				trace->select();
			}
			
		}

		signals_moved();
	}

	update();
}

void Header::leaveEvent(QEvent*)
{
	_mouse_point = QPoint(-1, -1);
	update();
}

void Header::contextMenuEvent(QContextMenuEvent *event)
{
	const shared_ptr<Trace> t = get_mouse_over_trace(_mouse_point);

	if (!t)
		return;

	QMenu menu(this);
	menu.addAction(_action_set_name);
	menu.addAction(_action_set_colour);

	_context_trace = t;
	menu.exec(event->globalPos());
	_context_trace.reset();
}

void Header::on_action_set_name_triggered()
{
	bool ok = false;

	shared_ptr<view::Trace> context_trace = _context_trace;
	if (!context_trace)
		return;

	const QString new_label = QInputDialog::getText(this, tr("Set Name"),
		tr("Name"), QLineEdit::Normal, context_trace->get_name(), &ok);

	if (ok)
		context_trace->set_name(new_label);
}

void Header::on_action_set_colour_triggered()
{
	shared_ptr<view::Trace> context_trace = _context_trace;
	if (!context_trace)
		return;

	const QColor new_colour = QColorDialog::getColor(
		context_trace->get_colour(), this, tr("Set Colour"));

	if (new_colour.isValid())
		context_trace->set_colour(new_colour);
}

void Header::on_signals_changed()
{
	const vector< shared_ptr<Trace> > traces(_view.get_traces());
	BOOST_FOREACH(shared_ptr<Trace> t, traces) {
		assert(t);
		connect(t.get(), SIGNAL(text_changed()), this, SLOT(update()));
	}
}

void Header::on_signals_moved()
{
	update();
}


} // namespace view
} // namespace pv
