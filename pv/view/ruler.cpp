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

#include "ruler.h"

#include "cursor.h"
#include "view.h"
#include "viewport.h"

#include <extdef.h>

#include <assert.h>
#include <math.h>
#include <limits.h>

#include <QMouseEvent>
#include <QPainter>
#include <QTextStream>

using namespace std;

namespace pv {
namespace view {

const int Ruler::MinorTickSubdivision = 4;
const int Ruler::ScaleUnits[3] = {1, 2, 5};

const QString Ruler::SIPrefixes[9] =
	{"f", "p", "n", QChar(0x03BC), "m", "", "k", "M", "G"};
const int Ruler::FirstSIPrefixPower = -15;

const int Ruler::HoverArrowSize = 5;

Ruler::Ruler(View &parent) :
	QWidget(&parent),
	_view(parent),
	_grabbed_marker(NULL)
{
	setMouseTracking(true);

	connect(&_view, SIGNAL(hover_point_changed()),
		this, SLOT(hover_point_changed()));
}

QString Ruler::format_time(double t, unsigned int prefix,
	unsigned int precision)
{
	const double multiplier = pow(10.0,
		- prefix * 3 - FirstSIPrefixPower);

	QString s;
	QTextStream ts(&s);
	ts.setRealNumberPrecision(precision);
	ts << fixed << forcesign << (t  * multiplier) <<
		SIPrefixes[prefix] << "s";
	return s;
}

void Ruler::paintEvent(QPaintEvent*)
{
	using namespace Qt;

	const double SpacingIncrement = 32.0f;
	const double MinValueSpacing = 32.0f;
	const int ValueMargin = 3;

	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);

	double min_width = SpacingIncrement, typical_width;
	double tick_period;
	unsigned int prefix;

	// Find tick spacing, and number formatting that does not cause
	// value to collide.
	do
	{
		const double min_period = _view.scale() * min_width;

		const int order = (int)floorf(log10f(min_period));
		const double order_decimal = pow(10, order);

		unsigned int unit = 0;

		do
		{
			tick_period = order_decimal * ScaleUnits[unit++];
		} while (tick_period < min_period && unit < countof(ScaleUnits));

		prefix = (order - FirstSIPrefixPower) / 3;
		assert(prefix < countof(SIPrefixes));


		typical_width = p.boundingRect(0, 0, INT_MAX, INT_MAX,
			AlignLeft | AlignTop, format_time(_view.offset(),
			prefix)).width() + MinValueSpacing;

		min_width += SpacingIncrement;

	} while(typical_width > tick_period / _view.scale());

	const int text_height = p.boundingRect(0, 0, INT_MAX, INT_MAX,
		AlignLeft | AlignTop, "8").height();

	// Draw the tick marks
	p.setPen(palette().color(foregroundRole()));

	const double minor_tick_period = tick_period / MinorTickSubdivision;
	const double first_major_division =
		floor(_view.offset() / tick_period);
	const double first_minor_division =
		ceil(_view.offset() / minor_tick_period);
	const double t0 = first_major_division * tick_period;

	int division = (int)round(first_minor_division -
		first_major_division * MinorTickSubdivision);

	const int major_tick_y1 = text_height + ValueMargin * 2;
	const int tick_y2 = height();
	const int minor_tick_y1 = (major_tick_y1 + tick_y2) / 2;

	while (1)
	{
		const double t = t0 + division * minor_tick_period;
		const double x = (t - _view.offset()) / _view.scale();

		if (x >= width())
			break;

		if (division % MinorTickSubdivision == 0)
		{
			// Draw a major tick
			p.drawText(x, ValueMargin, 0, text_height,
				AlignCenter | AlignTop | TextDontClip,
				format_time(t, prefix));
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
	}

	// Draw the cursors
	draw_cursors(p, prefix);

	// Draw the hover mark
	draw_hover_mark(p);

	p.end();
}

void Ruler::mouseMoveEvent(QMouseEvent *e)
{
	if (!_grabbed_marker)
		return;

	_grabbed_marker->set_time(_view.offset() +
		((double)e->x() + 0.5) * _view.scale());
}

void Ruler::mousePressEvent(QMouseEvent *e)
{
	if (e->buttons() & Qt::LeftButton) {
		_grabbed_marker = NULL;

		if (_view.cursors_shown()) {
			std::pair<Cursor, Cursor> &cursors =
				_view.cursors();
			if (cursors.first.get_label_rect(
				rect()).contains(e->pos()))
				_grabbed_marker = &cursors.first;
			else if (cursors.second.get_label_rect(
				rect()).contains(e->pos()))
				_grabbed_marker = &cursors.second;
		}
	}
}

void Ruler::mouseReleaseEvent(QMouseEvent *)
{
	_grabbed_marker = NULL;
}

void Ruler::draw_cursors(QPainter &p, unsigned int prefix)
{
	if (!_view.cursors_shown())
		return;

	const QRect r = rect();
	pair<Cursor, Cursor> &cursors = _view.cursors();
	cursors.first.paint_label(p, r, prefix);
	cursors.second.paint_label(p, r, prefix);
}

void Ruler::draw_hover_mark(QPainter &p)
{
	const int x = _view.hover_point().x();

	if (x == -1 || _grabbed_marker)
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

} // namespace view
} // namespace pv
