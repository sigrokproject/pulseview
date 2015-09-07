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

#include <algorithm>

#include <QRadialGradient>

#include "signal.hpp"
#include "signalscalehandle.hpp"
#include "tracetreeitemowner.hpp"

using std::max;
using std::min;

namespace pv {
namespace view {

SignalScaleHandle::SignalScaleHandle(Signal &owner) :
	owner_(owner)
{
}

bool SignalScaleHandle::enabled() const
{
	return selected() || owner_.selected();
}

void SignalScaleHandle::select(bool select)
{
	ViewItem::select(select);
	owner_.owner()->row_item_appearance_changed(true, true);
}

void SignalScaleHandle::drag_release()
{
	RowItem::drag_release();
	owner_.scale_handle_released();
	owner_.owner()->row_item_appearance_changed(true, true);
}

void SignalScaleHandle::drag_by(const QPoint &delta)
{
	owner_.scale_handle_dragged(
		drag_point_.y() + delta.y() - owner_.get_visual_y());
	owner_.owner()->row_item_appearance_changed(true, true);
}

QPoint SignalScaleHandle::point(const QRect &rect) const
{
	return owner_.point(rect) + QPoint(0, owner_.scale_handle_offset());
}

QRectF SignalScaleHandle::hit_box_rect(const ViewItemPaintParams &pp) const
{
	const int text_height = ViewItemPaintParams::text_height();
	const double x = -pp.pixels_offset() - text_height / 2;
	const double min_x = pp.left() + text_height;
	const double max_x = pp.right() - text_height * 2;
	return QRectF(min(max(x, min_x), max_x),
		owner_.get_visual_y() + owner_.scale_handle_offset() -
			text_height / 2,
		text_height, text_height);
}

void SignalScaleHandle::paint_fore(QPainter &p, const ViewItemPaintParams &pp)
{
	if (!enabled())
		return;

	const QRectF r(hit_box_rect(pp));
	const QPointF c = (r.topLeft() + 2 * r.center()) / 3;
	QRadialGradient gradient(c, r.width(), c);

	if (selected()) {
		gradient.setColorAt(0.0, QColor(255, 255, 255));
		gradient.setColorAt(0.75, QColor(192, 192, 192));
		gradient.setColorAt(1.0, QColor(128, 128, 128));
	} else {
		gradient.setColorAt(0.0, QColor(192, 192, 192));
		gradient.setColorAt(0.75, QColor(128, 128, 128));
		gradient.setColorAt(1.0, QColor(128, 128, 128));
	}

	p.setBrush(QBrush(gradient));
	p.setPen(QColor(128, 128, 128));
	p.drawEllipse(r);
}

} // view
} // pv
