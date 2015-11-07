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

#include "triggermarker.hpp"
#include "view.hpp"

namespace pv {
namespace view {

const QColor TriggerMarker::Colour(0x00, 0x00, 0xB0);

TriggerMarker::TriggerMarker(View &view, const pv::util::Timestamp& time) :
	TimeItem(view),
	time_(time)
{
}

TriggerMarker::TriggerMarker(const TriggerMarker &marker) :
	TimeItem(marker.view_),
	time_(marker.time_)
{
}

bool TriggerMarker::enabled() const
{
	return true;
}

bool TriggerMarker::is_draggable() const
{
	return false;
}

void TriggerMarker::set_time(const pv::util::Timestamp& time)
{
	time_ = time;

	view_.time_item_appearance_changed(true, true);
}

float TriggerMarker::get_x() const
{
	return ((time_ - view_.offset()) / view_.scale()).convert_to<float>();
}

QPoint TriggerMarker::point(const QRect &rect) const
{
	return QPoint(get_x(), rect.bottom());
}

void TriggerMarker::paint_fore(QPainter &p, const ViewItemPaintParams &pp)
{
	if (!enabled())
		return;

	QPen pen(Colour);
	pen.setStyle(Qt::DashLine);

	const float x = get_x();
	p.setPen(pen);
	p.drawLine(QPointF(x, pp.top()), QPointF(x, pp.bottom()));
}

} // namespace view
} // namespace pv
