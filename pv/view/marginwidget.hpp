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

#ifndef PULSEVIEW_PV_MARGINWIDGET_HPP
#define PULSEVIEW_PV_MARGINWIDGET_HPP

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
	 * Indicates the event an a view item has been clicked.
	 * @param item the view item that has been clicked.
	 */
	virtual void item_clicked(
		const std::shared_ptr<pv::view::ViewItem> &item);

	/**
	 * Shows the popup of a the specified @c ViewItem .
	 * @param item The item to show the popup for.
	 */
	void show_popup(const std::shared_ptr<ViewItem> &item);

protected:
	virtual void contextMenuEvent(QContextMenuEvent *event);

	virtual void keyPressEvent(QKeyEvent *event);
};

} // namespace view
} // namespace pv

#endif // PULSEVIEW_PV_MARGINWIDGET_HPP
