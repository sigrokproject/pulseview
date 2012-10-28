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
#include <QRect>
#include <QRectF>

namespace pv {
namespace view {

const QColor Cursor::LineColour(32, 74, 135);
const QColor Cursor::FillColour(52, 101, 164);
const QColor Cursor::HighlightColour(83, 130, 186);
const QColor Cursor::TextColour(Qt::white);

const int Cursor::Size = 12;
const int Cursor::Offset = 1;

Cursor::Cursor(const View &view, double time) :
	TimeMarker(view, LineColour, time)
{
}

void Cursor::paint_label(QPainter &p, const QRect &rect)
{
	const float x = (_time - _view.offset()) / _view.scale();
	const QRectF r(x - Size/2, rect.height() - Size - Offset,
		Size, Size);

	p.setPen(LineColour);
	p.setBrush(QBrush(FillColour));
	p.drawEllipse(r);

	p.setPen(HighlightColour);
	p.setBrush(QBrush());
	p.drawEllipse(r.adjusted(1, 1, -1, -1));
}

} // namespace view
} // namespace pv
