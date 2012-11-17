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
	QWidget(&parent),
	_view(parent),
	_action_set_name(new QAction(tr("Set &Name..."), this)),
	_action_set_colour(new QAction(tr("Set &Colour..."), this))
{
	setMouseTracking(true);

	connect(_action_set_name, SIGNAL(triggered()),
		this, SLOT(on_action_set_name_triggered()));
	connect(_action_set_colour, SIGNAL(triggered()),
		this, SLOT(on_action_set_colour_triggered()));
}

boost::shared_ptr<pv::view::Signal> Header::get_mouse_over_signal(
	const QPoint &pt)
{
	const int w = width();
	const vector< shared_ptr<Signal> > &sigs =
		_view.session().get_signals();

	const int v_offset = _view.v_offset();
	BOOST_FOREACH(const shared_ptr<Signal> s, sigs)
	{
		assert(s);

		const QRect signal_heading_rect(
			0, s->get_v_offset() - v_offset,
			w, View::SignalHeight);

		if(s->pt_in_label_rect(signal_heading_rect, pt))
			return s;
	}

	return shared_ptr<Signal>();
}

void Header::paintEvent(QPaintEvent *event)
{
	const int w = width();
	const vector< shared_ptr<Signal> > &sigs =
		_view.session().get_signals();

	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	const int v_offset = _view.v_offset();
	BOOST_FOREACH(const shared_ptr<Signal> s, sigs)
	{
		assert(s);

		const QRect signal_heading_rect(
			0, s->get_v_offset() - v_offset,
			w, View::SignalHeight);

		s->paint_label(painter, signal_heading_rect,
			s->pt_in_label_rect(signal_heading_rect, _mouse_point));
	}

	painter.end();
}

void Header::mousePressEvent(QMouseEvent *event)
{
	assert(event);

	const vector< shared_ptr<Signal> > &sigs =
		_view.session().get_signals();

	if(~QApplication::keyboardModifiers() & Qt::ControlModifier) {
		// Unselect all other signals because the Ctrl is not
		// pressed
		BOOST_FOREACH(const shared_ptr<Signal> s, sigs)
			s->select(false);
	}

	// Select the signal if it has been clicked
	const shared_ptr<Signal> mouse_over_signal =
		get_mouse_over_signal(event->pos());
	if(mouse_over_signal)
		mouse_over_signal->select(!mouse_over_signal->selected());

	update();
}

void Header::mouseMoveEvent(QMouseEvent *event)
{
	assert(event);
	_mouse_point = event->pos();
	update();
}

void Header::leaveEvent(QEvent *event)
{
	_mouse_point = QPoint(-1, -1);
	update();
}

void Header::contextMenuEvent(QContextMenuEvent *event)
{
	const shared_ptr<Signal> s = get_mouse_over_signal(_mouse_point);

	if(!s)
		return;

	QMenu menu(this);
	menu.addAction(_action_set_name);
	menu.addAction(_action_set_colour);

	_context_signal = s;
	menu.exec(event->globalPos());
	_context_signal.reset();
}

void Header::on_action_set_name_triggered()
{
	shared_ptr<view::Signal> context_signal = _context_signal;
	if(!context_signal)
		return;

	const QString new_label = QInputDialog::getText(this, tr("Set Name"),
		tr("Name"), QLineEdit::Normal, context_signal->get_name());

	if(!new_label.isEmpty())
		context_signal->set_name(new_label);
}

void Header::on_action_set_colour_triggered()
{
	shared_ptr<view::Signal> context_signal = _context_signal;
	if(!context_signal)
		return;

	const QColor new_colour = QColorDialog::getColor(
		context_signal->get_colour(), this, tr("Set Colour"));

	if(new_colour.isValid())
		context_signal->set_colour(new_colour);
}

} // namespace view
} // namespace pv
