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

#include "ruler.h"
#include "view.h"

#include <QBrush>
#include <QPainter>
#include <QPointF>
#include <QRect>
#include <QRectF>

#include <stdio.h>

#include <extdef.h>

using namespace boost;

namespace pv {
namespace view {

const QColor Cursor::LineColour(32, 74, 135);
const QColor Cursor::FillColour(52, 101, 164);
const QColor Cursor::HighlightColour(83, 130, 186);
const QColor Cursor::TextColour(Qt::white);

const int Cursor::Offset = 1;

const int Cursor::ArrowSize = 4;

Cursor::Cursor(const View &view, double time) :
	TimeMarker(view, LineColour, time)
{
}

QRectF Cursor::get_label_rect(const QRect &rect) const
{
	const shared_ptr<Cursor> other(get_other_cursor());
	assert(other);

	const float x = (_time - _view.offset()) / _view.scale();

	const QSizeF label_size(
		_text_size.width() + View::LabelPadding.width() * 2,
		_text_size.height() + View::LabelPadding.height() * 2);
	const float top = rect.height() - label_size.height() -
		Cursor::Offset - Cursor::ArrowSize - 0.5f;
	const float height = label_size.height();

	if (_time > other->time())
		return QRectF(x, top, label_size.width(), height);
	else
		return QRectF(x - label_size.width(), top,
			label_size.width(), height);
}

void Cursor::paint_label(QPainter &p, const QRect &rect,
	unsigned int prefix)
{
	const shared_ptr<Cursor> other(get_other_cursor());
	assert(other);

	compute_text_size(p, prefix);
	const QRectF r(get_label_rect(rect));

	const QPointF left_points[] = {
		r.topLeft(),
		r.topRight(),
		r.bottomRight(),
		QPointF(r.left() + ArrowSize, r.bottom()),
		QPointF(r.left(), rect.bottom()),
	};

	const QPointF right_points[] = {
		r.topRight(),
		r.topLeft(),
		r.bottomLeft(),
		QPointF(r.right() - ArrowSize, r.bottom()),
		QPointF(r.right(), rect.bottom()),
	};

	const QPointF left_highlight_points[] = {
		QPointF(r.left() + 1, r.top() + 1),
		QPointF(r.right() - 1, r.top() + 1),
		QPointF(r.right() - 1, r.bottom() - 1),
		QPointF(r.left() + ArrowSize - 1, r.bottom() - 1),
		QPointF(r.left() + 1, rect.bottom() - 1),
	};

	const QPointF right_highlight_points[] = {
		QPointF(r.right() - 1, r.top() + 1),
		QPointF(r.left() + 1, r.top() + 1),
		QPointF(r.left() + 1, r.bottom() - 1),
		QPointF(r.right() - ArrowSize + 1, r.bottom() - 1),
		QPointF(r.right() - 1, rect.bottom() - 1),
	};

	const QPointF *const points = (_time > other->time()) ?
		left_points : right_points;
	const QPointF *const highlight_points = (_time > other->time()) ?
		left_highlight_points : right_highlight_points;

	if (selected()) {
		p.setPen(highlight_pen());
		p.setBrush(Qt::transparent);
		p.drawPolygon(points, countof(left_points));
	}

	p.setPen(Qt::transparent);
	p.setBrush(FillColour);
	p.drawPolygon(points, countof(left_points));

	p.setPen(HighlightColour);
	p.setBrush(Qt::transparent);
	p.drawPolygon(highlight_points, countof(left_highlight_points));

	p.setPen(LineColour);
	p.setBrush(Qt::transparent);
	p.drawPolygon(points, countof(left_points));

	p.setPen(TextColour);
	p.drawText(r, Qt::AlignCenter | Qt::AlignVCenter,
		Ruler::format_time(_time, prefix, 2));
}

void Cursor::compute_text_size(QPainter &p, unsigned int prefix)
{
	_text_size = p.boundingRect(QRectF(), 0,
		Ruler::format_time(_time, prefix, 2)).size();
}

shared_ptr<Cursor> Cursor::get_other_cursor() const
{
	const CursorPair &cursors = _view.cursors();
	return (cursors.first().get() == this) ?
		cursors.second() : cursors.first();
}

} // namespace view
} // namespace pv
