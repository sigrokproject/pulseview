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

#include <assert.h>

#include <QtGui>
#include <QApplication>
#include <QDesktopWidget>

#include "popup.h"

using std::max;
using std::min;

namespace pv {
namespace widgets {

const unsigned int Popup::ArrowLength = 10;
const unsigned int Popup::ArrowOverlap = 3;
const unsigned int Popup::MarginWidth = 6;

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

bool Popup::space_for_arrow() const
{
	// Check if there is room for the arrow
	switch (_pos) {
	case Right:
		if (_point.x() > x())
			return false;
		return true;

	case Bottom:
		if (_point.y() > y())
			return false;
		return true;		

	case Left:
		if (_point.x() < (x() + width()))
			return false;
		return true;

	case Top:
		if (_point.y() < (y() + height()))
			return false;
		return true;
	}

	return true;
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
	if (space_for_arrow())
		return arrow_region().united(bubble_region());
	else
		return bubble_region();
}

void Popup::reposition_widget()
{
	QPoint o;

	const QRect screen_rect = QApplication::desktop()->availableGeometry(
		QApplication::desktop()->screenNumber(_point));

	if (_pos == Right || _pos == Left)
		o.ry() = -height() / 2;
	else
		o.rx() = -width() / 2;

	if (_pos == Left)
		o.rx() = -width();
	else if(_pos == Top)
		o.ry() = -height();

	o += _point;
	move(max(min(o.x(), screen_rect.right() - width()),
			screen_rect.left()),
		max(min(o.y(), screen_rect.bottom() - height()),
			screen_rect.top()));
}

void Popup::closeEvent(QCloseEvent*)
{
	closed();
}

void Popup::paintEvent(QPaintEvent*)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	const QColor outline_color(QApplication::palette().color(
		QPalette::Dark));

	// Draw the bubble
	const QRegion b = bubble_region();
	const QRegion bubble_outline = QRegion(rect()).subtracted(
		b.translated(1, 0).intersected(b.translated(0, 1).intersected(
		b.translated(-1, 0).intersected(b.translated(0, -1)))));

	painter.setPen(Qt::NoPen);
	painter.setBrush(QApplication::palette().brush(QPalette::Window));
	painter.drawRect(rect());

	// Draw the arrow
	if (!space_for_arrow())
		return;

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

