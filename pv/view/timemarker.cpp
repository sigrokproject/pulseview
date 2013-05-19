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

#include "timemarker.h"

#include "view.h"

#include <QPainter>

using namespace std;

namespace pv {
namespace view {

TimeMarker::TimeMarker(View &view, const QColor &colour, double time) :
	_view(view),
	_colour(colour),
	_time(time),
	_value_action(&view),
	_value_widget(&view),
	_updating_value_widget(false)
{
	_value_action.setDefaultWidget(&_value_widget);

	_value_widget.setValue(time);
	_value_widget.setDecimals(6);
	_value_widget.setSuffix("s");
	_value_widget.setSingleStep(1e-6);

	connect(&_value_widget, SIGNAL(valueChanged(double)),
		this, SLOT(on_value_changed(double)));
}

double TimeMarker::time() const
{
	return _time;
}

void TimeMarker::set_time(double time)
{
	_time = time;
	_updating_value_widget = true;
	_value_widget.setValue(time);
	_updating_value_widget = false;
	time_changed();
}

void TimeMarker::paint(QPainter &p, const QRect &rect)
{
	const float x = (_time - _view.offset()) / _view.scale();
	p.setPen(_colour);
	p.drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
}

const list<QAction*> TimeMarker::get_context_bar_actions()
{
	list<QAction*> actions;
	actions.push_back(&_value_action);
	return actions;
}

void TimeMarker::on_value_changed(double value)
{
	if (!_updating_value_widget) {
		_time = value;
		time_changed();
	}
}

} // namespace view
} // namespace pv
