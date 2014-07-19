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

#ifndef PULSEVIEW_PV_VIEW_RULER_H
#define PULSEVIEW_PV_VIEW_RULER_H

#include <memory>

#include "marginwidget.h"

namespace pv {
namespace view {

class Ruler : public MarginWidget
{
	Q_OBJECT

private:
	static const int RulerHeight;
	static const int MinorTickSubdivision;
	static const int ScaleUnits[3];

	static const int HoverArrowSize;

public:
	Ruler(View &parent);

	/**
	 * Find a tick spacing and number formatting that does not cause
	 * the values to collide.
	 * @param p A QPainter used to determine the needed space for the values.
	 * @param scale A pv::view::View's scale.
	 * @param offset A pv::view::View's offset.
	 *
	 * @return The tick period to use in 'first' and the prefix in 'second'.
	 */
	static std::pair<double, unsigned int> calculate_tick_spacing(
		QPainter& p, double scale, double offset);

public:
	QSize sizeHint() const;

private:
	void paintEvent(QPaintEvent *event);

private:
	/**
	 * Draw a hover arrow under the cursor position.
	 */
	void draw_hover_mark(QPainter &p);

private Q_SLOTS:
	void hover_point_changed();
};

} // namespace view
} // namespace pv

#endif // PULSEVIEW_PV_VIEW_RULER_H
