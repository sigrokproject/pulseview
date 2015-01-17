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

#ifndef PULSEVIEW_PV_VIEW_VIEWPORT_HPP
#define PULSEVIEW_PV_VIEW_VIEWPORT_HPP

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

private:
	/**
	 * Indicates when a view item is being hovered over.
	 * @param item The item that is being hovered over, or @c nullptr
	 * if no view item is being hovered over.
	 */
	void item_hover(const std::shared_ptr<pv::view::ViewItem> &item);

	/**
	 * Gets the first view item which has a hit-box that contains @c pt .
	 * @param pt the point to search with.
	 * @return the view item that has been found, or and empty
	 *   @c shared_ptr if no item was found.
	 */
	std::shared_ptr<pv::view::ViewItem> get_mouse_over_item(
		const QPoint &pt);

	/**
	 * Sets this item into the dragged state.
	 */
	void drag();

	/**
	 * Drag the background by the delta offset.
	 * @param delta the drag offset in pixels.
	 */
	void drag_by(const QPoint &delta);

	/**
	 * Sets this item into the un-dragged state.
	 */
	void drag_release();

	/**
	 * Gets the items in the view widget.
	 */
	std::vector< std::shared_ptr<pv::view::ViewItem> > items();

	/**
	 * Handles touch begin update and end events.
	 * @param e the event that triggered this handler.
	 */
	bool touch_event(QTouchEvent *e);

private:
	void paintEvent(QPaintEvent *event);

	void mouseDoubleClickEvent(QMouseEvent * event);
	void wheelEvent(QWheelEvent *event);

private:
	double drag_offset_;

	double pinch_offset0_;
	double pinch_offset1_;
	bool pinch_zoom_active_;
};

} // namespace view
} // namespace pv

#endif // PULSEVIEW_PV_VIEW_VIEWPORT_HPP
