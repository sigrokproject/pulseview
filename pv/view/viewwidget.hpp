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

#ifndef PULSEVIEW_PV_VIEWWIDGET_H
#define PULSEVIEW_PV_VIEWWIDGET_H

#include <QWidget>

namespace pv {
namespace view {

class View;

class ViewWidget : public QWidget
{
	Q_OBJECT

protected:
	ViewWidget(View &parent);

	/**
	 * Returns true if the selection of row items allows dragging.
	 * @return Returns true if the drag is acceptable.
	 */
	bool accept_drag() const;

	/**
	 * Drag the dragging items by the delta offset.
	 * @param delta the drag offset in pixels.
	 */
	void drag_items(const QPoint &delta);

protected:
	pv::view::View &view_;
};

} // namespace view
} // namespace pv

#endif // PULSEVIEW_PV_VIEWWIDGET_H
