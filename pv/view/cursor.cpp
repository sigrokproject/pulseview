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

#include "cursor.hpp"

#include "ruler.hpp"
#include "view.hpp"
#include "pv/util.hpp"

#include <QApplication>
#include <QBrush>
#include <QPainter>
#include <QPointF>
#include <QRect>
#include <QRectF>

#include <cassert>
#include <cstdio>
#include <limits>

using std::abs;
using std::shared_ptr;
using std::numeric_limits;

namespace pv {
namespace views {
namespace TraceView {

const QColor Cursor::FillColour(52, 101, 164);

Cursor::Cursor(View &view, double time) :
	TimeMarker(view, FillColour, time)
{
}

bool Cursor::enabled() const
{
	return view_.cursors_shown();
}

QString Cursor::get_text() const
{
	const shared_ptr<Cursor> other = get_other_cursor();
	const pv::util::Timestamp& diff = abs(time_ - other->time_);

	return Ruler::format_time_with_distance(
		diff, time_, view_.tick_prefix(), view_.time_unit(), view_.tick_precision());
}

QRectF Cursor::label_rect(const QRectF &rect) const
{
	const shared_ptr<Cursor> other(get_other_cursor());
	assert(other);

	const float x = ((time_ - view_.offset())/ view_.scale()).convert_to<float>();

	QFontMetrics m(QApplication::font());
	QSize text_size = m.boundingRect(get_text()).size();

	const QSizeF label_size(
		text_size.width() + LabelPadding.width() * 2,
		text_size.height() + LabelPadding.height() * 2);
	const float top = rect.height() - label_size.height() -
		TimeMarker::ArrowSize - 0.5f;
	const float height = label_size.height();

	const pv::util::Timestamp& other_time = other->time();

	if (time_ > other_time ||
		(abs(time_ - other_time).is_zero() && this > other.get()))
		return QRectF(x, top, label_size.width(), height);
	else
		return QRectF(x - label_size.width(), top, label_size.width(), height);
}

shared_ptr<Cursor> Cursor::get_other_cursor() const
{
	const shared_ptr<CursorPair> cursors(view_.cursors());
	assert(cursors);
	return (cursors->first().get() == this) ?
		cursors->second() : cursors->first();
}

} // namespace TraceView
} // namespace views
} // namespace pv
