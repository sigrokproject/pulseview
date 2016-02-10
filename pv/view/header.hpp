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

#ifndef PULSEVIEW_PV_VIEW_HEADER_HPP
#define PULSEVIEW_PV_VIEW_HEADER_HPP

#include <list>
#include <memory>
#include <utility>

#include "marginwidget.hpp"

namespace pv {
namespace view {

class TraceTreeItem;
class View;
class ViewItem;

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

	/**
	 * The horizontal offset, relative to the left edge of the widget,
	 * where the arrows of the trace labels end.
	 */
	static const int BaselineOffset;

private:
	/**
	 * Gets the row items.
	 */
	std::vector< std::shared_ptr<pv::view::ViewItem> > items();

	/**
	 * Gets the first view item which has a label that contains @c pt .
	 * @param pt the point to search with.
	 * @return the view item that has been found, or and empty
	 *   @c shared_ptr if no item was found.
	 */
	std::shared_ptr<pv::view::ViewItem> get_mouse_over_item(
		const QPoint &pt);

private:
	void paintEvent(QPaintEvent *event);

private:
	void contextMenuEvent(QContextMenuEvent *event);

	void keyPressEvent(QKeyEvent *event);

private Q_SLOTS:
	void on_group();

	void on_ungroup();
};

} // namespace view
} // namespace pv

#endif // PULSEVIEW_PV_VIEW_HEADER_HPP
