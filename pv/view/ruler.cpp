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

#include <extdef.h>

#include <QApplication>
#include <QFontMetrics>
#include <QMouseEvent>

#include "ruler.hpp"
#include "view.hpp"

using namespace Qt;

using std::shared_ptr;
using std::vector;

namespace pv {
namespace view {

const float Ruler::RulerHeight = 2.5f;  // x Text Height
const int Ruler::MinorTickSubdivision = 4;

const float Ruler::HoverArrowSize = 0.5f;  // x Text Height

Ruler::Ruler(View &parent) :
	MarginWidget(parent)
{
	setMouseTracking(true);

	connect(&view_, SIGNAL(hover_point_changed()),
		this, SLOT(hover_point_changed()));
	connect(&view_, SIGNAL(offset_changed()),
		this, SLOT(invalidate_tick_position_cache()));
	connect(&view_, SIGNAL(scale_changed()),
		this, SLOT(invalidate_tick_position_cache()));
	connect(&view_, SIGNAL(tick_prefix_changed()),
		this, SLOT(invalidate_tick_position_cache()));
	connect(&view_, SIGNAL(tick_precision_changed()),
		this, SLOT(invalidate_tick_position_cache()));
	connect(&view_, SIGNAL(tick_period_changed()),
		this, SLOT(invalidate_tick_position_cache()));
	connect(&view_, SIGNAL(time_unit_changed()),
		this, SLOT(invalidate_tick_position_cache()));
}

QSize Ruler::sizeHint() const
{
	const int text_height = calculate_text_height();
	return QSize(0, RulerHeight * text_height);
}

QSize Ruler::extended_size_hint() const
{
	QRectF max_rect;
	std::vector< std::shared_ptr<TimeItem> > items(view_.time_items());
	for (auto &i : items)
		max_rect = max_rect.united(i->label_rect(QRect()));
	return QSize(0, sizeHint().height() - max_rect.top() / 2 +
		ViewItem::HighlightRadius);
}

QString Ruler::format_time_with_distance(
	const pv::util::Timestamp& distance,
	const pv::util::Timestamp& t,
	pv::util::SIPrefix prefix,
	pv::util::TimeUnit unit,
	unsigned precision,
	bool sign)
{
	const unsigned limit = 60;

	if (t.is_zero())
		return "0";

	// If we have to use samples then we have no alternative formats
	if (unit == pv::util::TimeUnit::Samples)
		return pv::util::format_time_si_adjusted(t, prefix, precision, "sa", sign);

	// View zoomed way out -> low precision (0), big distance (>=60s)
	// -> DD:HH:MM
	if ((precision == 0) && (distance >= limit))
		return pv::util::format_time_minutes(t, 0, sign);

	// View in "normal" range -> medium precision, medium step size
	// -> HH:MM:SS.mmm... or xxxx (si unit) if less than limit seconds
	// View zoomed way in -> high precision (>3), low step size (<1s)
	// -> HH:MM:SS.mmm... or xxxx (si unit) if less than limit seconds
	if (abs(t) < limit)
		return pv::util::format_time_si_adjusted(t, prefix, precision, "s", sign);
	else
		return pv::util::format_time_minutes(t, precision, sign);
}

vector< shared_ptr<ViewItem> > Ruler::items()
{
	const vector< shared_ptr<TimeItem> > time_items(view_.time_items());
	return vector< shared_ptr<ViewItem> >(
		time_items.begin(), time_items.end());
}

shared_ptr<ViewItem> Ruler::get_mouse_over_item(const QPoint &pt)
{
	const vector< shared_ptr<TimeItem> > items(view_.time_items());
	for (auto i = items.rbegin(); i != items.rend(); i++)
		if ((*i)->enabled() && (*i)->label_rect(rect()).contains(pt))
			return *i;
	return nullptr;
}

void Ruler::paintEvent(QPaintEvent*)
{
	if (!tick_position_cache_) {
		auto ffunc = [this](const pv::util::Timestamp& t)
		{
			return format_time_with_distance(
				this->view_.tick_period(),
				t,
				this->view_.tick_prefix(),
				this->view_.time_unit(),
				this->view_.tick_precision());
		};

		tick_position_cache_.emplace(calculate_tick_positions(
			view_.tick_period(),
			view_.offset(),
			view_.scale(),
			width(),
			ffunc));
	}

	const int ValueMargin = 3;

	const int text_height = calculate_text_height();
	const int ruler_height = RulerHeight * text_height;
	const int major_tick_y1 = text_height + ValueMargin * 2;
	const int minor_tick_y1 = (major_tick_y1 + ruler_height) / 2;

	QPainter p(this);

	// Draw the tick marks
	p.setPen(palette().color(foregroundRole()));

	for (const auto& tick: tick_position_cache_->major) {
		p.drawText(tick.first, ValueMargin, 0, text_height,
				AlignCenter | AlignTop | TextDontClip, tick.second);
		p.drawLine(QPointF(tick.first, major_tick_y1),
			QPointF(tick.first, ruler_height));
	}

	for (const auto& tick: tick_position_cache_->minor) {
		p.drawLine(QPointF(tick, minor_tick_y1),
				QPointF(tick, ruler_height));
	}

	// Draw the hover mark
	draw_hover_mark(p, text_height);

	p.setRenderHint(QPainter::Antialiasing);

	// The cursor labels are not drawn with the arrows exactly on the
	// bottom line of the widget, because then the selection shadow
	// would be clipped away.
	const QRect r = rect().adjusted(0, 0, 0, -ViewItem::HighlightRadius);

	// Draw the items
	const vector< shared_ptr<TimeItem> > items(view_.time_items());
	for (auto &i : items) {
		const bool highlight = !item_dragging_ &&
			i->label_rect(r).contains(mouse_point_);
		i->paint_label(p, r, highlight);
	}
}

Ruler::TickPositions Ruler::calculate_tick_positions(
	const pv::util::Timestamp& major_period,
	const pv::util::Timestamp& offset,
	const double scale,
	const int width,
	std::function<QString(const pv::util::Timestamp&)> format_function)
{
	TickPositions tp;

	const double minor_period =
		(major_period / MinorTickSubdivision).convert_to<double>();
	const pv::util::Timestamp first_major_division = floor(offset / major_period);
	const pv::util::Timestamp first_minor_division = ceil(offset / minor_period);
	const pv::util::Timestamp t0 = first_major_division * major_period;

	int division = (round(first_minor_division -
		first_major_division * MinorTickSubdivision)).convert_to<int>() - 1;

	double x;

	do {
		pv::util::Timestamp t = t0 + division * minor_period;
		x = ((t - offset) / scale).convert_to<double>();

		if (division % MinorTickSubdivision == 0) {
			// Recalculate 't' without using 'minor_period' which is of type double.
			t = t0 + division / MinorTickSubdivision * major_period;
			tp.major.emplace_back(x, format_function(t));
		} else {
			tp.minor.emplace_back(x);
		}

		division++;
	} while (x < width);

	return tp;
}

void Ruler::mouseDoubleClickEvent(QMouseEvent *e)
{
	view_.add_flag(view_.offset() + ((double)e->x() + 0.5) * view_.scale());
}

void Ruler::draw_hover_mark(QPainter &p, int text_height)
{
	const int x = view_.hover_point().x();

	if (x == -1)
		return;

	p.setPen(QPen(Qt::NoPen));
	p.setBrush(QBrush(palette().color(foregroundRole())));

	const int b = RulerHeight * text_height;
	const float hover_arrow_size = HoverArrowSize * text_height;
	const QPointF points[] = {
		QPointF(x, b),
		QPointF(x - hover_arrow_size, b - hover_arrow_size),
		QPointF(x + hover_arrow_size, b - hover_arrow_size)
	};
	p.drawPolygon(points, countof(points));
}

int Ruler::calculate_text_height() const
{
	return QFontMetrics(font()).ascent();
}

void Ruler::hover_point_changed()
{
	update();
}

void Ruler::invalidate_tick_position_cache()
{
	tick_position_cache_ = boost::none;
}

void Ruler::resizeEvent(QResizeEvent*)
{
	// the tick calculation depends on the width of this widget
	invalidate_tick_position_cache();
}

} // namespace view
} // namespace pv
