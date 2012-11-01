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

#include "cursor.h"

#include "view.h"

#include <QBrush>
#include <QPainter>
#include <QPointF>
#include <QRect>
#include <QRectF>

#include <stdio.h>

#include <extdef.h>

namespace pv {
namespace view {

const QColor Cursor::LineColour(32, 74, 135);
const QColor Cursor::FillColour(52, 101, 164);
const QColor Cursor::HighlightColour(83, 130, 186);
const QColor Cursor::TextColour(Qt::white);

const int Cursor::Size = 12;
const int Cursor::Offset = 1;

const int Cursor::ArrowSize = 4;

Cursor::Cursor(const View &view, double time) :
	TimeMarker(view, LineColour, time)
{
}

QRectF Cursor::get_label_rect(const QRect &rect) const
{
	const float x = (_time - _view.offset()) / _view.scale();

	const QSizeF label_size(
		_text_size.width() + View::LabelPadding.width() * 2,
		_text_size.height() + View::LabelPadding.height() * 2);
	return QRectF(x - label_size.width() / 2 - 0.5f,
		rect.height() - label_size.height() - Offset - ArrowSize - 0.5f,
		label_size.width() + 1, label_size.height() + 1);
}

void Cursor::paint_label(QPainter &p, const QRect &rect)
{
	compute_text_size(p);
	const QRectF r(get_label_rect(rect));

	const float h_centre = (r.left() + r.right()) / 2;
	const QPointF points[] = {
		r.topRight(),
		QPointF(r.right(), r.bottom()),
		QPointF(h_centre + ArrowSize, r.bottom()),
		QPointF(h_centre, rect.bottom()),
		QPointF(h_centre - ArrowSize, r.bottom()),
		QPointF(r.left(), r.bottom()),
		r.topLeft()
	};

	const QPointF highlight_points[] = {
		QPointF(r.right() - 1, r.top() + 1),
		QPointF(r.right() - 1, r.bottom() - 1),
		QPointF(h_centre + ArrowSize - 1, r.bottom() - 1),
		QPointF(h_centre, rect.bottom() - 1),
		QPointF(h_centre - ArrowSize + 1, r.bottom() - 1),
		QPointF(r.left() + 1, r.bottom() - 1),
		QPointF(r.left() + 1, r.top() + 1),
	};

	char text[16];
	format_text(text);

	p.setPen(Qt::transparent);
	p.setBrush(FillColour);
	p.drawPolygon(points, countof(points));

	p.setPen(HighlightColour);
	p.setBrush(Qt::transparent);
	p.drawPolygon(highlight_points, countof(highlight_points));

	p.setPen(LineColour);
	p.setBrush(Qt::transparent);
	p.drawPolygon(points, countof(points));

	p.setPen(TextColour);
	p.drawText(r, Qt::AlignCenter | Qt::AlignVCenter, text);
}

void Cursor::compute_text_size(QPainter &p)
{
	char text[16];
	format_text(text);
	_text_size = p.boundingRect(QRectF(), 0, text).size();
}

void Cursor::format_text(char *text)
{
	sprintf(text, "%gs", _time);
}

} // namespace view
} // namespace pv
