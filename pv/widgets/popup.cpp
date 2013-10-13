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

#include <algorithm>

#include <QtGui>

#include "popup.h"

using namespace std;

namespace pv {
namespace widgets {

const unsigned int Popup::ArrowLength = 15;
const unsigned int Popup::ArrowOverlap = 3;
const unsigned int Popup::MarginWidth = 10;

Popup::Popup(QWidget *parent) :
	QWidget(parent, Qt::Popup | Qt::FramelessWindowHint),
	_point(),
	_pos(Left)
{
}

const QPoint& Popup::point() const
{
	return _point;
}

Popup::Position Popup::position() const
{
	return _pos;
}

void Popup::set_position(const QPoint point, Position pos)
{
	_point = point, _pos = pos;

	setContentsMargins(
		MarginWidth + ((pos == Right) ? ArrowLength : 0),
		MarginWidth + ((pos == Bottom) ? ArrowLength : 0),
		MarginWidth + ((pos == Left) ? ArrowLength : 0),
		MarginWidth + ((pos == Top) ? ArrowLength : 0));

}

QPolygon Popup::arrow_polygon() const
{
	QPolygon poly;

	const QPoint p = mapFromGlobal(_point);
	const int l = ArrowLength + ArrowOverlap; 

	switch (_pos)
        {
	case Right:
		poly << QPoint(p.x() + l, p.y() - l);
		break;

	case Bottom:
		poly << QPoint(p.x() - l, p.y() + l);
		break;

        case Left:
	case Top:
		poly << QPoint(p.x() - l, p.y() - l);
		break;
	}

	poly << p;

	switch (_pos)
        {
	case Right:
	case Bottom:
		poly << QPoint(p.x() + l, p.y() + l);
		break;

        case Left:
		poly << QPoint(p.x() - l, p.y() + l);
		break;
		
	case Top:
		poly << QPoint(p.x() + l, p.y() - l);
		break;
	}

	return poly;
}

QRegion Popup::arrow_region() const
{
	return QRegion(arrow_polygon());
}

QRect Popup::bubble_rect() const
{
	return QRect(
		QPoint((_pos == Right) ? ArrowLength : 0,
			(_pos == Bottom) ? ArrowLength : 0),
		QSize(width() - ((_pos == Left || _pos == Right) ?
				ArrowLength : 0),
			height() - ((_pos == Top || _pos == Bottom) ?
				ArrowLength : 0)));
}

QRegion Popup::bubble_region() const
{
	const QRect rect(bubble_rect());

	const unsigned int r = MarginWidth;
	const unsigned int d = 2 * r;
	return QRegion(rect.adjusted(r, 0, -r, 0)).united(
		QRegion(rect.adjusted(0, r, 0, -r))).united(
		QRegion(rect.left(), rect.top(), d, d,
			QRegion::Ellipse)).united(
		QRegion(rect.right() - d, rect.top(), d, d,
			QRegion::Ellipse)).united(
		QRegion(rect.left(), rect.bottom() - d, d, d,
			QRegion::Ellipse)).united(
		QRegion(rect.right() - d, rect.bottom() - d, d, d,
			QRegion::Ellipse));
}

QRegion Popup::popup_region() const
{
	return arrow_region().united(bubble_region());
}

void Popup::reposition_widget()
{
	QPoint o;

	if (_pos == Right || _pos == Left)
		o.ry() = -height() / 2;
	else
		o.rx() = -width() / 2;

	if (_pos == Left)
		o.rx() = -width();
	else if(_pos == Top)
		o.ry() = -height();

	move(_point + o);
}

void Popup::paintEvent(QPaintEvent*)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	const QColor outline_color(QApplication::palette().color(
		QPalette::Dark));

	const QRegion b = bubble_region();
	const QRegion bubble_outline = QRegion(rect()).subtracted(
		b.translated(1, 0).intersected(b.translated(0, 1).intersected(
		b.translated(-1, 0).intersected(b.translated(0, -1)))));

	painter.setPen(Qt::NoPen);
	painter.setBrush(QApplication::palette().brush(QPalette::Window));
	painter.drawRect(rect());

	const QPoint ArrowOffsets[] = {
		QPoint(1, 0), QPoint(0, -1), QPoint(-1, 0), QPoint(0, 1)};

	const QRegion a(arrow_region());
	const QRegion arrow_outline = a.subtracted(
		a.translated(ArrowOffsets[_pos]));

	painter.setClipRegion(bubble_outline.subtracted(a).united(
		arrow_outline));
	painter.setBrush(outline_color);
	painter.drawRect(rect());
}

void Popup::resizeEvent(QResizeEvent*)
{
	reposition_widget();
	setMask(popup_region());
}

void Popup::mouseReleaseEvent(QMouseEvent *e)
{
	assert(e);

	// We need our own out-of-bounds click handler because QWidget counts
	// the drop-shadow region as inside the widget
	if(!bubble_rect().contains(e->pos()))
		close();
}

void Popup::showEvent(QShowEvent*)
{
	reposition_widget();
}

} // namespace widgets
} // namespace pv

