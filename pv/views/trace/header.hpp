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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PULSEVIEW_PV_VIEWS_TRACEVIEW_HEADER_HPP
#define PULSEVIEW_PV_VIEWS_TRACEVIEW_HEADER_HPP

#include <list>
#include <memory>
#include <utility>

#include "marginwidget.hpp"

using std::shared_ptr;
using std::vector;

namespace pv {
namespace views {
namespace trace {

class TraceTreeItem;
class View;
class ViewItem;

/**
 * The Header class provides an area for @ref Trace labels to be shown,
 * trace-related settings to be edited, trace groups to be shown and similar.
 * Essentially, it is the main management area of the @ref View itself and
 * shown on the left-hand side of the trace area.
 */
class Header : public MarginWidget
{
	Q_OBJECT

private:
	static const int Padding;

public:
	Header(View &parent);

	QSize sizeHint() const;

	/**
	 * The extended area that the header widget would like to be sized to.
	 * @remarks This area is the area specified by sizeHint, extended by
	 * the area to overlap the viewport.
	 */
	QSize extended_size_hint() const;

private:
	/**
	 * Gets the row items.
	 */
	vector< shared_ptr<ViewItem> > items();

	/**
	 * Gets the first view item which has a label that contains @c pt .
	 * @param pt the point to search with.
	 * @return the view item that has been found, or and empty
	 *   @c shared_ptr if no item was found.
	 */
	shared_ptr<ViewItem> get_mouse_over_item(const QPoint &pt);

private:
	void paintEvent(QPaintEvent *event);

private:
	void contextMenuEvent(QContextMenuEvent *event);

	void keyPressEvent(QKeyEvent *event);

private Q_SLOTS:
	void on_group();

	void on_ungroup();
};

} // namespace trace
} // namespace views
} // namespace pv

#endif // PULSEVIEW_PV_VIEWS_TRACEVIEW_HEADER_HPP
