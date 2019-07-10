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

#include "pv/util.hpp"
#include "ruler.hpp"
#include "view.hpp"

#include <QApplication>
#include <QBrush>
#include <QMenu>
#include <QPainter>
#include <QPointF>
#include <QRect>
#include <QRectF>

#include <cassert>
#include <cstdio>
#include <limits>

using std::abs; // NOLINT. Force usage of std::abs() instead of C's abs().
using std::shared_ptr;

namespace pv {
namespace views {
namespace trace {

const QColor Cursor::FillColor(52, 101, 164);

Cursor::Cursor(View &view, double time) :
	TimeMarker(view, FillColor, time)
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
		diff, view_.ruler()->get_ruler_time_from_absolute_time(time_),
        view_.tick_prefix(), view_.time_unit(), view_.tick_precision());
}

QRectF Cursor::label_rect(const QRectF &rect) const
{
	const shared_ptr<Cursor> other(get_other_cursor());
	assert(other);

	const float x = get_x();

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

QMenu *Cursor::create_header_context_menu(QWidget *parent)
{
	QMenu *const menu = new QMenu(parent);

	QAction *const snap_disable = new QAction(tr("Disable snapping"), this);
	snap_disable->setCheckable(true);
	snap_disable->setChecked(snapping_disabled_);
	connect(snap_disable, &QAction::toggled, this, [=](bool checked){snapping_disabled_ = checked;});
	menu->addAction(snap_disable);

	return menu;
}

shared_ptr<Cursor> Cursor::get_other_cursor() const
{
	const shared_ptr<CursorPair> cursors(view_.cursors());
	assert(cursors);
	return (cursors->first().get() == this) ?
		cursors->second() : cursors->first();
}

} // namespace trace
} // namespace views
} // namespace pv
