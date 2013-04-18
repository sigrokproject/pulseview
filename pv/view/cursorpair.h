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

#ifndef PULSEVIEW_PV_VIEW_CURSORPAIR_H
#define PULSEVIEW_PV_VIEW_CURSORPAIR_H

#include "cursor.h"

#include <QPainter>

class QPainter;

namespace pv {
namespace view {

class CursorPair
{
public:
	/**
	 * Constructor.
	 * @param view A reference to the view that owns this cursor pair.
	 */
	CursorPair(const View &view);

	/**
	 * Returns a reference to the first cursor.
	 */
	Cursor& first();

	/**
	 * Returns a reference to the first cursor.
	 */
	const Cursor& first() const;

	/**
	 * Returns a reference to the second cursor.
	 */
	Cursor& second();

	/**
	 * Returns a reference to the second cursor.
	 */
	const Cursor& second() const;

public:
	void draw_viewport_background(QPainter &p, const QRect &rect);

	void draw_viewport_foreground(QPainter &p, const QRect &rect);

private:
	Cursor _first, _second;
	const View &_view;
};

} // namespace view
} // namespace pv

#endif // PULSEVIEW_PV_VIEW_CURSORPAIR_H
