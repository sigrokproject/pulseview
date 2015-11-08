/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2014 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#ifndef PULSEVIEW_PV_VIEW_TRIGGER_MARKER_HPP
#define PULSEVIEW_PV_VIEW_TRIGGER_MARKER_HPP

#include "timeitem.hpp"

namespace pv {
namespace view {

class TriggerMarker : public TimeItem
{
	Q_OBJECT

public:
	static const QColor Colour;

public:
	/**
	 * Constructor.
	 * @param view A reference to the view that owns this marker.
	 * @param time The time to set the marker to.
	 */
	TriggerMarker(View &view, const pv::util::Timestamp& time);

	/**
	 * Copy constructor.
	 */
	TriggerMarker(const TriggerMarker &marker);

	/**
	 * Returns true if the item is visible and enabled.
	 */
	bool enabled() const override;

	/**
	  Returns true if the item may be dragged/moved.
	 */
	bool is_draggable() const override;

	/**
	 * Sets the time of the marker.
	 */
	void set_time(const pv::util::Timestamp& time) override;

	float get_x() const override;

	/**
	 * Gets the arrow-tip point of the time marker.
	 * @param rect the rectangle of the ruler area.
	 */
	QPoint point(const QRect &rect) const override;

	/**
	 * Paints the foreground layer of the item with a QPainter
	 * @param p the QPainter to paint into.
	 * @param pp the painting parameters object to paint with.
	 */
	void paint_fore(QPainter &p, const ViewItemPaintParams &pp) override;

private:
	pv::util::Timestamp time_;
};

} // namespace view
} // namespace pv

#endif // PULSEVIEW_PV_VIEW_TRIGGER_MARKER_HPP
