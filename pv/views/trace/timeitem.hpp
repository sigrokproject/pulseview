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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PULSEVIEW_PV_VIEWS_TRACEVIEW_TIMEITEM_HPP
#define PULSEVIEW_PV_VIEWS_TRACEVIEW_TIMEITEM_HPP

#include "viewitem.hpp"

namespace pv {
namespace views {
namespace trace {

class View;

/**
 * The TimeItem class represents items on the @ref Ruler. It is generic in
 * nature, not making assumptions about the kind of item shown.
 */
class TimeItem : public ViewItem
{
	Q_OBJECT

protected:
	/**
	 * Constructor.
	 * @param view A reference to the view that owns this marker.
	 */
	TimeItem(View &view);

public:
	/**
	 * Sets the time of the marker.
	 */
	virtual void set_time(const pv::util::Timestamp& time) = 0;

	virtual const pv::util::Timestamp time() const = 0;

	virtual float get_x() const = 0;

	virtual const pv::util::Timestamp delta(const pv::util::Timestamp& other) const = 0;

	/**
	 * Drags the item to a delta relative to the drag point.
	 * @param delta the offset from the drag point.
	 */
	void drag_by(const QPoint &delta);

protected:
	View &view_;
};

} // namespace trace
} // namespace views
} // namespace pv

#endif // PULSEVIEW_PV_VIEWS_TRACEVIEW_TIMEITEM_HPP
