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

#ifndef PULSEVIEW_PV_VIEW_VIEWPORT_H
#define PULSEVIEW_PV_VIEW_VIEWPORT_H

#include <QTimer>
#include <QTouchEvent>

#include "viewwidget.hpp"

class QPainter;
class QPaintEvent;
class Session;

namespace pv {
namespace view {

class View;

class Viewport : public ViewWidget
{
	Q_OBJECT

public:
	explicit Viewport(View &parent);

protected:
	void paintEvent(QPaintEvent *event);

private:
	bool event(QEvent *event);
	void mousePressEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	void mouseDoubleClickEvent(QMouseEvent * event);
	void wheelEvent(QWheelEvent *event);
	bool touchEvent(QTouchEvent *e);

private:
	QPoint mouse_down_point_;
	double mouse_down_offset_;
	bool mouse_down_valid_;

	double pinch_offset0_;
	double pinch_offset1_;
	bool pinch_zoom_active_;
};

} // namespace view
} // namespace pv

#endif // PULSEVIEW_PV_VIEW_VIEWPORT_H
