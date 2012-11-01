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

namespace pv {
namespace view {

TimeMarker::TimeMarker(const View &view, const QColor &colour,
	double time) :
	_view(view),
	_colour(colour),
	_time(time)
{
}

TimeMarker::TimeMarker(const TimeMarker &s) :
	_view(s._view),
	_colour(s._colour),
	_time(s._time)
{
}

double TimeMarker::time() const
{
	return _time;
}

void TimeMarker::set_time(double time)
{
	_time = time;
}

void TimeMarker::paint(QPainter &p, const QRect &rect)
{
	const float x = (_time - _view.offset()) / _view.scale();
	p.setPen(_colour);
	p.drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
}

} // namespace view
} // namespace pv
