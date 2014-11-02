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

#ifndef PULSEVIEW_PV_VIEW_HEADER_H
#define PULSEVIEW_PV_VIEW_HEADER_H

#include <list>
#include <memory>
#include <utility>

#include "marginwidget.h"

namespace pv {
namespace view {

class RowItem;
class View;

class Header : public MarginWidget
{
	Q_OBJECT

private:
	static const int Padding;

public:
	Header(View &parent);

	QSize sizeHint() const;

	/**
	 * The horizontal offset, relative to the left edge of the widget,
	 * where the arrows of the trace labels end.
	 */
	static const int BaselineOffset;

private:
	std::shared_ptr<pv::view::RowItem> get_mouse_over_row_item(
		const QPoint &pt);

	void clear_selection();

	void show_popup(const std::shared_ptr<RowItem> &item);

private:
	void paintEvent(QPaintEvent *event);

private:
	void mouseLeftPressEvent(QMouseEvent *event);
	void mousePressEvent(QMouseEvent * event);

	void mouseLeftReleaseEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);

	void mouseMoveEvent(QMouseEvent *event);

	void leaveEvent(QEvent *event);

	void contextMenuEvent(QContextMenuEvent *event);

	void keyPressEvent(QKeyEvent *e);

private Q_SLOTS:
	void on_signals_moved();

	void on_group();

Q_SIGNALS:
	void signals_moved();

private:
	QPoint _mouse_point;
	QPoint _mouse_down_point;
	std::shared_ptr<RowItem> _mouse_down_item;
	bool _dragging;
};

} // namespace view
} // namespace pv

#endif // PULSEVIEW_PV_VIEW_HEADER_H
