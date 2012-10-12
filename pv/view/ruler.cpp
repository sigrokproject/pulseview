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
#include "view.h"

#include "../../extdef.h"

#include <assert.h>
#include <math.h>

#include <QPainter>
#include <QTextStream>

namespace pv {
namespace view {

const int Ruler::MinorTickSubdivision = 4;
const int Ruler::ScaleUnits[3] = {1, 2, 5};

const QString Ruler::SIPrefixes[9] =
	{"f", "p", "n", QChar(0x03BC), "m", "", "k", "M", "G"};
const int Ruler::FirstSIPrefixPower = -15;

Ruler::Ruler(View &parent) :
	QWidget(&parent),
	_view(parent)
{
}

void Ruler::paintEvent(QPaintEvent *event)
{
	QPainter p(this);

	const double MinSpacing = 80;

	const double min_period = _view.scale() * MinSpacing;

	const int order = (int)floorf(log10f(min_period));
	const double order_decimal = pow(10, order);

	int unit = 0;
	double tick_period = 0.0f;

	do
	{
		tick_period = order_decimal * ScaleUnits[unit++];
	} while(tick_period < min_period && unit < countof(ScaleUnits));

	const int prefix = (order - FirstSIPrefixPower) / 3;
	assert(prefix >= 0);
	assert(prefix < countof(SIPrefixes));

	const double multiplier = pow(0.1, prefix * 3 + FirstSIPrefixPower);

	const int text_height = p.boundingRect(0, 0, INT_MAX, INT_MAX,
		Qt::AlignLeft | Qt::AlignTop, "8").height();

	// Draw the tick marks
	p.setPen(Qt::black);

	const double minor_tick_period = tick_period / MinorTickSubdivision;
	const double first_major_division =
		floor(_view.offset() / tick_period);
	const double first_minor_division =
		ceil(_view.offset() / minor_tick_period);
	const double t0 = first_major_division * tick_period;

	int division = (int)round(first_minor_division -
		first_major_division * MinorTickSubdivision);
	while(1)
	{
		const double t = t0 + division * minor_tick_period;
		const double x = (t - _view.offset()) / _view.scale();

		if(x >= width())
			break;

		if(division % MinorTickSubdivision == 0)
		{
			// Draw a major tick
			QString s;
			QTextStream ts(&s);
			ts << (t * multiplier) << SIPrefixes[prefix] << "s";
			p.drawText(x, 0, 0, text_height, Qt::AlignCenter |
				Qt::AlignTop | Qt::TextDontClip, s);
			p.drawLine(x, text_height, x, height());
		}
		else
		{
			// Draw a minor tick
			p.drawLine(x, (text_height + height()) / 2,
				x, height());
		}

		division++;
	}

	p.end();
}

} // namespace view
} // namespace pv
