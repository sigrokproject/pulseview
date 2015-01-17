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

#ifndef PULSEVIEW_PV_VIEW_CURSOR_HPP
#define PULSEVIEW_PV_VIEW_CURSOR_HPP

#include "timemarker.hpp"

#include <memory>

#include <QSizeF>

class QPainter;

namespace pv {
namespace view {

class Cursor : public TimeMarker
{
	Q_OBJECT

public:
	static const QColor FillColour;

public:
	/**
	 * Constructor.
	 * @param view A reference to the view that owns this cursor pair.
	 * @param time The time to set the flag to.
	 */
	Cursor(View &view, double time);

public:
	/**
	 * Returns true if the item is visible and enabled.
	 */
	bool enabled() const;

	/**
	 * Gets the text to show in the marker.
	 */
	QString get_text() const;

	/**
	 * Gets the marker label rectangle.
	 * @param rect The rectangle of the ruler client area.
	 * @return Returns the label rectangle.
	 */
	QRectF label_rect(const QRectF &rect) const;

private:
	std::shared_ptr<Cursor> get_other_cursor() const;
};

} // namespace view
} // namespace pv

#endif // PULSEVIEW_PV_VIEW_CURSOR_HPP
