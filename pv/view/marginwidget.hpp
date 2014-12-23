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

#ifndef PULSEVIEW_PV_MARGINWIDGET_H
#define PULSEVIEW_PV_MARGINWIDGET_H

#include <memory>

#include <QPoint>

#include "viewwidget.hpp"

namespace pv {
namespace view {

class ViewItem;

class MarginWidget : public ViewWidget
{
	Q_OBJECT

public:
	MarginWidget(pv::view::View &parent);

	/**
	 * The extended area that the margin widget would like to be sized to.
	 * @remarks This area is the area specified by sizeHint, extended by
	 * the area to overlap the viewport.
	 */
	virtual QSize extended_size_hint() const = 0;

protected:
	/**
	 * Gets the items in the margin widget.
	 */
	virtual std::vector< std::shared_ptr<pv::view::ViewItem> > items() = 0;

	/**
	 * Gets the first view item which has a label that contains @c pt .
	 * @param pt the point to search with.
	 * @return the view item that has been found, or and empty
	 *   @c shared_ptr if no item was found.
	 */
	virtual std::shared_ptr<pv::view::ViewItem> get_mouse_over_item(
		const QPoint &pt) = 0;

	/**
	 * Shows the popup of a the specified @c ViewItem .
	 * @param item The item to show the popup for.
	 */
	void show_popup(const std::shared_ptr<ViewItem> &item);

	/**
	 * Drag the dragging items by the delta offset.
	 * @param delta the drag offset in pixels.
	 */
	virtual void drag_items(const QPoint &delta) = 0;

	/**
	 * Handles left mouse button press events.
	 * @param event the mouse event that triggered this handler.
	 */
	void mouse_left_press_event(QMouseEvent *event);

	/**
	 * Handles left mouse button release events.
	 * @param event the mouse event that triggered this handler.
	 */
	void mouse_left_release_event(QMouseEvent *event);

protected:
	void mousePressEvent(QMouseEvent * event);
	void mouseReleaseEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);

	void leaveEvent(QEvent *event);

	virtual void contextMenuEvent(QContextMenuEvent *event);

	virtual void keyPressEvent(QKeyEvent *e);

public Q_SLOTS:
	void clear_selection();

Q_SIGNALS:
	void selection_changed();

protected:
	QPoint mouse_point_;
	QPoint mouse_down_point_;
	std::shared_ptr<ViewItem> mouse_down_item_;
	bool dragging_;
};

} // namespace view
} // namespace pv

#endif // PULSEVIEW_PV_MARGINWIDGET_H
