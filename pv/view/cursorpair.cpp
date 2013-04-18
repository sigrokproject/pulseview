/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2013 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#include "cursorpair.h"

#include "view.h"

#include <algorithm>

using namespace std;

namespace pv {
namespace view {

CursorPair::CursorPair(const View &view) :
	_first(view, 0.0),
	_second(view, 1.0),
	_view(view)
{
}

const Cursor& CursorPair::first() const
{
	return _first;
}

Cursor& CursorPair::first()
{
	return _first;
}

const Cursor& CursorPair::second() const
{
	return _second;
}

Cursor& CursorPair::second()
{
	return _second;
}

void CursorPair::draw_markers(QPainter &p,
	const QRect &rect, unsigned int prefix)
{
	_first.paint_label(p, rect, prefix);
	_second.paint_label(p, rect, prefix);
}

void CursorPair::draw_viewport_background(QPainter &p,
	const QRect &rect)
{
	p.setPen(Qt::NoPen);
	p.setBrush(QBrush(View::CursorAreaColour));

	const float x1 = (_first.time() - _view.offset()) / _view.scale();
	const float x2 = (_second.time() - _view.offset()) / _view.scale();
	const int l = (int)max(min(x1, x2), 0.0f);
	const int r = (int)min(max(x1, x2), (float)rect.width());

	p.drawRect(l, 0, r - l, rect.height());
}

void CursorPair::draw_viewport_foreground(QPainter &p,
	const QRect &rect)
{
	_first.paint(p, rect);
	_second.paint(p, rect);
}

} // namespace view
} // namespace pv
