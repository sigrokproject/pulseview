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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PULSEVIEW_PV_VIEWWIDGET_HPP
#define PULSEVIEW_PV_VIEWWIDGET_HPP

#include <memory>

#include <QPoint>
#include <QWidget>

#include <pv/util.hpp>

using std::shared_ptr;
using std::vector;

class QTouchEvent;

namespace pv {
namespace views {
namespace trace {

class View;
class ViewItem;

class ViewWidget : public QWidget
{
	Q_OBJECT

protected:
	ViewWidget(View &parent);

	/**
	 * Indicates when a view item is being hovered over.
	 * @param item The item that is being hovered over, or @c nullptr
	 * if no view item is being hovered over.
	 * @remarks the default implementation does nothing.
	 */
	virtual void item_hover(const shared_ptr<ViewItem> &item, QPoint pos);

	/**
	 * Indicates the event an a view item has been clicked.
	 * @param item the view item that has been clicked.
	 * @remarks the default implementation does nothing.
	 */
	virtual void item_clicked(const shared_ptr<ViewItem> &item);

	/**
	 * Returns true if the selection of row items allows dragging.
	 * @return Returns true if the drag is acceptable.
	 */
	bool accept_drag() const;

	/**
	 * Returns true if the mouse button is down.
	 */
	bool mouse_down() const;

	/**
	 * Drag the dragging items by the delta offset.
	 * @param delta the drag offset in pixels.
	 */
	void drag_items(const QPoint &delta);

	/**
	 * Sets this item into the dragged state.
	 */
	virtual void drag();

	/**
	 * Drag the background by the delta offset.
	 * @param delta the drag offset in pixels.
	 * @remarks The default implementation does nothing.
	 */
	virtual void drag_by(const QPoint &delta);

	/**
	 * Sets this item into the un-dragged state.
	 */
	virtual void drag_release();

	/**
	 * Gets the items in the view widget.
	 */
	virtual vector< shared_ptr<ViewItem> > items() = 0;

	/**
	 * Gets the first view item which has a hit-box that contains @c pt .
	 * @param pt the point to search with.
	 * @return the view item that has been found, or and empty
	 *   @c shared_ptr if no item was found.
	 */
	virtual shared_ptr<ViewItem> get_mouse_over_item(const QPoint &pt) = 0;

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

	/**
	 * Handles touch begin update and end events.
	 * @param e the event that triggered this handler.
	 */
	virtual bool touch_event(QTouchEvent *event);

protected:
	bool event(QEvent *event);

	void mousePressEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);

	void keyPressEvent(QKeyEvent *event);
	void keyReleaseEvent(QKeyEvent *event);

	void leaveEvent(QEvent *event);

public Q_SLOTS:
	void clear_selection();

Q_SIGNALS:
	void selection_changed();

protected:
	pv::views::trace::View &view_;
	QPoint mouse_point_;
	QPoint mouse_down_point_;
	pv::util::Timestamp mouse_down_offset_;
	shared_ptr<ViewItem> mouse_down_item_;

	/// Keyboard modifiers that were active when mouse was last moved or clicked
	Qt::KeyboardModifiers mouse_modifiers_;

	bool item_dragging_;
};

} // namespace trace
} // namespace views
} // namespace pv

#endif // PULSEVIEW_PV_VIEWWIDGET_HPP
