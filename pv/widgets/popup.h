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

#ifndef PULSEVIEW_PV_WIDGETS_POPUP_H
#define PULSEVIEW_PV_WIDGETS_POPUP_H

#include <QWidget>

namespace pv {
namespace widgets {

class Popup : public QWidget
{
public:
	enum Position
	{
		Right,
		Top,
		Left,
		Bottom
	};

private:
	static const unsigned int ArrowLength;
	static const unsigned int ArrowOverlap;
	static const unsigned int MarginWidth;

public:
	Popup(QWidget *parent);

	const QPoint& point() const;
	Position position() const;

	void set_position(const QPoint point, Position pos);

private:
	QPolygon arrow_polygon() const;

	QRegion arrow_region() const;

	QRect bubble_rect() const;

	QRegion bubble_region() const;

	QRegion popup_region() const;

	void reposition_widget();

	void paintEvent(QPaintEvent*);

	void resizeEvent(QResizeEvent*);

	void mouseReleaseEvent(QMouseEvent *e);

protected:
	void showEvent(QShowEvent *e);

private:
	QPoint _point;
	Position _pos;
};

} // namespace widgets
} // namespace pv

#endif // PULSEVIEW_PV_WIDGETS_POPUP_H
