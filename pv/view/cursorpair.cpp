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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "cursorpair.hpp"

#include "ruler.hpp"
#include "view.hpp"
#include "pv/util.hpp"

#include <cassert>
#include <algorithm>

using std::max;
using std::make_pair;
using std::min;
using std::shared_ptr;
using std::pair;

namespace pv {
namespace views {
namespace TraceView {

const int CursorPair::DeltaPadding = 8;
const QColor CursorPair::ViewportFillColour(220, 231, 243);

CursorPair::CursorPair(View &view) :
	TimeItem(view),
	first_(new Cursor(view, 0.0)),
	second_(new Cursor(view, 1.0))
{
}

bool CursorPair::enabled() const
{
	return view_.cursors_shown();
}

shared_ptr<Cursor> CursorPair::first() const
{
	return first_;
}

shared_ptr<Cursor> CursorPair::second() const
{
	return second_;
}

void CursorPair::set_time(const pv::util::Timestamp& time)
{
	const pv::util::Timestamp delta = second_->time() - first_->time();
	first_->set_time(time);
	second_->set_time(time + delta);
}

float CursorPair::get_x() const
{
	return (first_->get_x() + second_->get_x()) / 2.0f;
}

QPoint CursorPair::point(const QRect &rect) const
{
	return first_->point(rect);
}

pv::widgets::Popup* CursorPair::create_popup(QWidget *parent)
{
	(void)parent;
	return nullptr;
}

QRectF CursorPair::label_rect(const QRectF &rect) const
{
	const QSizeF label_size(text_size_ + LabelPadding * 2);
	const pair<float, float> offsets(get_cursor_offsets());
	const pair<float, float> normal_offsets(
		(offsets.first < offsets.second) ? offsets :
		make_pair(offsets.second, offsets.first));

	const float height = label_size.height();
	const float left = max(normal_offsets.first + DeltaPadding, -height);
	const float right = min(normal_offsets.second - DeltaPadding,
		(float)rect.width() + height);

	return QRectF(left, rect.height() - label_size.height() -
		TimeMarker::ArrowSize - 0.5f,
		right - left, height);
}

void CursorPair::paint_label(QPainter &p, const QRect &rect, bool hover)
{
	assert(first_);
	assert(second_);

	if (!enabled())
		return;

	const QColor text_colour =
		ViewItem::select_text_colour(Cursor::FillColour);

	p.setPen(text_colour);
	compute_text_size(p);
	QRectF delta_rect(label_rect(rect));

	const int radius = delta_rect.height() / 2;
	const QRectF text_rect(delta_rect.intersected(
		rect).adjusted(radius, 0, -radius, 0));
	if (text_rect.width() >= text_size_.width()) {
		const int highlight_radius = delta_rect.height() / 2 - 2;

		if (selected()) {
			p.setBrush(Qt::transparent);
			p.setPen(highlight_pen());
			p.drawRoundedRect(delta_rect, radius, radius);
		}

		p.setBrush(hover ? Cursor::FillColour.lighter() :
			Cursor::FillColour);
		p.setPen(Cursor::FillColour.darker());
		p.drawRoundedRect(delta_rect, radius, radius);

		delta_rect.adjust(1, 1, -1, -1);
		p.setPen(Cursor::FillColour.lighter());
		p.drawRoundedRect(delta_rect, highlight_radius, highlight_radius);

		p.setPen(text_colour);
		p.drawText(text_rect, Qt::AlignCenter | Qt::AlignVCenter,
			format_string());
	}
}

void CursorPair::paint_back(QPainter &p, const ViewItemPaintParams &pp)
{
	if (!enabled())
		return;

	p.setPen(Qt::NoPen);
	p.setBrush(QBrush(ViewportFillColour));

	const pair<float, float> offsets(get_cursor_offsets());
	const int l = (int)max(min(
		offsets.first, offsets.second), 0.0f);
	const int r = (int)min(max(
		offsets.first, offsets.second), (float)pp.width());

	p.drawRect(l, pp.top(), r - l, pp.height());
}

QString CursorPair::format_string()
{
	const pv::util::SIPrefix prefix = view_.tick_prefix();
	const pv::util::Timestamp diff = abs(second_->time() - first_->time());

	const QString s1 = Ruler::format_time_with_distance(
		diff, diff, prefix, view_.time_unit(), view_.tick_precision(), false);
	const QString s2 = util::format_time_si(
		1 / diff, pv::util::SIPrefix::unspecified, 4, "Hz", false);

	return QString("%1 / %2").arg(s1).arg(s2);
}

void CursorPair::compute_text_size(QPainter &p)
{
	assert(first_);
	assert(second_);

	text_size_ = p.boundingRect(QRectF(), 0, format_string()).size();
}

pair<float, float> CursorPair::get_cursor_offsets() const
{
	assert(first_);
	assert(second_);

	return pair<float, float>(
		((first_->time() - view_.offset()) / view_.scale()).convert_to<float>(),
		((second_->time() - view_.offset()) / view_.scale()).convert_to<float>());
}

} // namespace TraceView
} // namespace views
} // namespace pv
