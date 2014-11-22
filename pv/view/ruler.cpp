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

#include "ruler.hpp"

#include "view.hpp"
#include "pv/util.hpp"

#include <extdef.h>

using namespace Qt;

namespace pv {
namespace view {

const int Ruler::RulerHeight = 30;
const int Ruler::MinorTickSubdivision = 4;
const int Ruler::ScaleUnits[3] = {1, 2, 5};

const int Ruler::HoverArrowSize = 5;

Ruler::Ruler(View &parent) :
	MarginWidget(parent)
{
	setMouseTracking(true);

	connect(&view_, SIGNAL(hover_point_changed()),
		this, SLOT(hover_point_changed()));
}

QSize Ruler::sizeHint() const
{
	return QSize(0, RulerHeight);
}

void Ruler::paintEvent(QPaintEvent*)
{
	const int ValueMargin = 3;

	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);

	std::pair<double, unsigned int> spacing =
		calculate_tick_spacing(p, view_.scale(), view_.offset());

	double tick_period = spacing.first;
	unsigned int prefix = spacing.second;

	const int text_height = p.boundingRect(0, 0, INT_MAX, INT_MAX,
		AlignLeft | AlignTop, "8").height();

	// Draw the tick marks
	p.setPen(palette().color(foregroundRole()));

	const double minor_tick_period = tick_period / MinorTickSubdivision;
	const double first_major_division =
		floor(view_.offset() / tick_period);
	const double first_minor_division =
		ceil(view_.offset() / minor_tick_period);
	const double t0 = first_major_division * tick_period;

	int division = (int)round(first_minor_division -
		first_major_division * MinorTickSubdivision) - 1;

	const int major_tick_y1 = text_height + ValueMargin * 2;
	const int tick_y2 = height();
	const int minor_tick_y1 = (major_tick_y1 + tick_y2) / 2;

	double x;

	do {
		const double t = t0 + division * minor_tick_period;
		x = (t - view_.offset()) / view_.scale();

		if (division % MinorTickSubdivision == 0)
		{
			// Draw a major tick
			p.drawText(x, ValueMargin, 0, text_height,
				AlignCenter | AlignTop | TextDontClip,
				pv::util::format_time(t, prefix));
			p.drawLine(QPointF(x, major_tick_y1),
				QPointF(x, tick_y2));
		}
		else
		{
			// Draw a minor tick
			p.drawLine(QPointF(x, minor_tick_y1),
				QPointF(x, tick_y2));
		}

		division++;

	} while (x < width());

	// Draw the hover mark
	draw_hover_mark(p);
}

void Ruler::draw_hover_mark(QPainter &p)
{
	const int x = view_.hover_point().x();

	if (x == -1)
		return;

	p.setPen(QPen(Qt::NoPen));
	p.setBrush(QBrush(palette().color(foregroundRole())));

	const int b = height() - 1;
	const QPointF points[] = {
		QPointF(x, b),
		QPointF(x - HoverArrowSize, b - HoverArrowSize),
		QPointF(x + HoverArrowSize, b - HoverArrowSize)
	};
	p.drawPolygon(points, countof(points));
}

void Ruler::hover_point_changed()
{
	update();
}

std::pair<double, unsigned int> Ruler::calculate_tick_spacing(
	QPainter& p, double scale, double offset)
{
	const double SpacingIncrement = 32.0f;
	const double MinValueSpacing = 32.0f;

	double min_width = SpacingIncrement, typical_width;

	double tick_period;
	unsigned int prefix;

	do {
		const double min_period = scale * min_width;

		const int order = (int)floorf(log10f(min_period));
		const double order_decimal = pow(10.0, order);

		unsigned int unit = 0;

		do {
			tick_period = order_decimal * ScaleUnits[unit++];
		} while (tick_period < min_period && unit < countof(ScaleUnits));

		prefix = (order - pv::util::FirstSIPrefixPower) / 3;

		typical_width = p.boundingRect(0, 0, INT_MAX, INT_MAX,
			AlignLeft | AlignTop, pv::util::format_time(offset,
			prefix)).width() + MinValueSpacing;

		min_width += SpacingIncrement;

	} while(typical_width > tick_period / scale);

	return std::make_pair(tick_period, prefix);
}

} // namespace view
} // namespace pv
