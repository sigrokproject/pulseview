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

#include <extdef.h>

#include <assert.h>
#include <math.h>

#include <QApplication>

#include "signal.h"
#include "view.h"

namespace pv {
namespace view {

const int Signal::LabelHitPadding = 2;
const int Signal::LabelHighlightRadius = 6;

const QPen Signal::SignalAxisPen(QColor(128, 128, 128, 64));

Signal::Signal(const sr_probe *const probe) :
	_probe(probe),
	_name(probe->name),
	_v_offset(0),
	_selected(false)
{
	assert(_probe);
}

QString Signal::get_name() const
{
	return _name;
}

void Signal::set_name(QString name)
{
	_name = name;
}

QColor Signal::get_colour() const
{
	return _colour;
}

void Signal::set_colour(QColor colour)
{
	_colour = colour;
}

int Signal::get_v_offset() const
{
	return _v_offset;
}

void Signal::set_v_offset(int v_offset)
{
	_v_offset = v_offset;
}

bool Signal::selected() const
{
	return _selected;
}

void Signal::select(bool select)
{
	_selected = select;
}

void Signal::paint_label(QPainter &p, int y, int right, bool hover)
{
	p.setBrush(_colour);

	if (!_probe->enabled)
		return;

	const QColor colour = get_colour();

	compute_text_size(p);
	const QRectF label_rect = get_label_rect(y, right);

	// Paint the label
	const QPointF points[] = {
		label_rect.topLeft(),
		label_rect.topRight(),
		QPointF(right, y),
		label_rect.bottomRight(),
		label_rect.bottomLeft()
	};

	const QPointF highlight_points[] = {
		QPointF(label_rect.left() + 1, label_rect.top() + 1),
		QPointF(label_rect.right(), label_rect.top() + 1),
		QPointF(right - 1, y),
		QPointF(label_rect.right(), label_rect.bottom() - 1),
		QPointF(label_rect.left() + 1, label_rect.bottom() - 1)
	};

	if (_selected) {
		p.setPen(QPen(QApplication::palette().brush(
			QPalette::Highlight), LabelHighlightRadius,
			Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
		p.setBrush(Qt::transparent);
		p.drawPolygon(points, countof(points));
	}

	p.setPen(Qt::transparent);
	p.setBrush(hover ? colour.lighter() : colour);
	p.drawPolygon(points, countof(points));

	p.setPen(colour.lighter());
	p.setBrush(Qt::transparent);
	p.drawPolygon(highlight_points, countof(highlight_points));

	p.setPen(colour.darker());
	p.setBrush(Qt::transparent);
	p.drawPolygon(points, countof(points));

	// Paint the text
	p.setPen((colour.lightness() > 64) ? Qt::black : Qt::white);
	p.drawText(label_rect, Qt::AlignCenter | Qt::AlignVCenter, _name);
}

bool Signal::pt_in_label_rect(int y, int left, int right,
	const QPoint &point)
{
	(void)left;

	const QRectF label = get_label_rect(y, right);
	return QRectF(
		QPointF(label.left() - LabelHitPadding,
			label.top() - LabelHitPadding),
		QPointF(right, label.bottom() + LabelHitPadding)
			).contains(point);
}

void Signal::paint_axis(QPainter &p, int y, int left, int right)
{
	p.setPen(SignalAxisPen);
	p.drawLine(QPointF(left, y + 0.5f), QPointF(right, y + 0.5f));
}

void Signal::compute_text_size(QPainter &p)
{
	_text_size = QSize(
		p.boundingRect(QRectF(), 0, _name).width(),
		p.boundingRect(QRectF(), 0, "Tg").height());
}

QRectF Signal::get_label_rect(int y, int right)
{
	using pv::view::View;

	const QSizeF label_size(
		_text_size.width() + View::LabelPadding.width() * 2,
		ceilf((_text_size.height() + View::LabelPadding.height() * 2) / 2) * 2);
	const float label_arrow_length = label_size.height() / 2;
	return QRectF(
		right - label_arrow_length - label_size.width() - 0.5,
		y + 0.5f - label_size.height() / 2,
		label_size.width(), label_size.height());
}

} // namespace view
} // namespace pv
