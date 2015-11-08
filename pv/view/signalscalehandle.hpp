/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2015 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#ifndef PULSEVIEW_PV_VIEW_SIGNALSCALEHANDLE_HPP
#define PULSEVIEW_PV_VIEW_SIGNALSCALEHANDLE_HPP

#include "rowitem.hpp"

namespace pv {
namespace view {

class Signal;

/**
 * A row item owned by a @c Signal that implements the v-scale adjustment grab
 * handle.
 */
class SignalScaleHandle : public RowItem
{
	Q_OBJECT
public:
	/**
	 * Constructor
	 */
	explicit SignalScaleHandle(Signal &owner);

public:
	/**
	 * Returns true if the parent item is enabled.
	 */
	bool enabled() const;

	/**
	 * Selects or deselects the signal.
	 */
	void select(bool select = true);

	/**
	 * Sets this item into the un-dragged state.
	 */
	void drag_release();

	/**
	 * Drags the item to a delta relative to the drag point.
	 * @param delta the offset from the drag point.
	 */
	void drag_by(const QPoint &delta);

	/**
	 * Get the drag point.
	 * @param rect the rectangle of the widget area.
	 */
	QPoint point(const QRect &rect) const;

	/**
	 * Computes the outline rectangle of the viewport hit-box.
	 * @param rect the rectangle of the viewport area.
	 * @return Returns the rectangle of the hit-box.
	 */
	QRectF hit_box_rect(const ViewItemPaintParams &pp) const;

	/**
	 * Paints the foreground layer of the item with a QPainter
	 * @param p the QPainter to paint into.
	 * @param pp the painting parameters object to paint with.
	 */
	void paint_fore(QPainter &p, const ViewItemPaintParams &pp);

private:
	Signal &owner_;
};

} // namespace view
} // namespace pv

#endif // PULSEVIEW_PV_VIEW_SIGNALSCALEHANDLE_HPP
