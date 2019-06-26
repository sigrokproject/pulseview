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

#include <algorithm>
#include <cmath>

#include <extdef.h>

#include "timemarker.hpp"

#include "pv/widgets/timestampspinbox.hpp"
#include "ruler.hpp"
#include "view.hpp"

#include <QApplication>
#include <QFontMetrics>
#include <QFormLayout>
#include <QPainter>

#include <pv/widgets/popup.hpp>

using std::max;
using std::min;

namespace pv {
namespace views {
namespace trace {

const int TimeMarker::ArrowSize = 4;

TimeMarker::TimeMarker(
	View &view, const QColor &color, const pv::util::Timestamp& time) :
	TimeItem(view),
	color_(color),
	time_(time),
	value_action_(nullptr),
	value_widget_(nullptr),
	updating_value_widget_(false)
{
}

const pv::util::Timestamp& TimeMarker::time() const
{
	return time_;
}

void TimeMarker::set_time(const pv::util::Timestamp& time)
{
	time_ = time;

	if (value_widget_) {
		updating_value_widget_ = true;
		value_widget_->setValue(view_.ruler()->get_ruler_time_from_absolute_time(time));
		updating_value_widget_ = false;
	}

	view_.time_item_appearance_changed(true, true);
}

float TimeMarker::get_x() const
{
	// Use roundf() from cmath, std::roundf() causes Android issues (see #945).
	return roundf(((time_ - view_.offset()) / view_.scale()).convert_to<float>()) + 0.5f;
}

QPoint TimeMarker::drag_point(const QRect &rect) const
{
	(void)rect;

	return QPoint(get_x(), view_.mapFromGlobal(QCursor::pos()).y());
}

QRectF TimeMarker::label_rect(const QRectF &rect) const
{
	QFontMetrics m(QApplication::font());
	const QSizeF text_size(
		max(m.boundingRect(get_text()).size().width(), ArrowSize),
		m.height());
	const QSizeF label_size(text_size + LabelPadding * 2);
	const float top = rect.height() - label_size.height() -
		TimeMarker::ArrowSize - 0.5f;
	const float x = get_x();

	return QRectF(QPointF(x - label_size.width() / 2, top), label_size);
}

QRectF TimeMarker::hit_box_rect(const ViewItemPaintParams &pp) const
{
	const float x = get_x();
	const float h = QFontMetrics(QApplication::font()).height();
	return QRectF(x - h / 2.0f, pp.top(), h, pp.height());
}

void TimeMarker::paint_label(QPainter &p, const QRect &rect, bool hover)
{
	if (!enabled())
		return;

	const qreal x = get_x();
	const QRectF r(label_rect(rect));

	const QPointF points[] = {
		r.topLeft(),
		r.bottomLeft(),
		QPointF(max(r.left(), x - ArrowSize), r.bottom()),
		QPointF(x, rect.bottom()),
		QPointF(min(r.right(), x + ArrowSize), r.bottom()),
		r.bottomRight(),
		r.topRight()
	};

	const QPointF highlight_points[] = {
		QPointF(r.left() + 1, r.top() + 1),
		QPointF(r.left() + 1, r.bottom() - 1),
		QPointF(max(r.left() + 1, x - ArrowSize), r.bottom() - 1),
		QPointF(min(max(r.left() + 1, x), r.right() - 1),
			rect.bottom() - 1),
		QPointF(min(r.right() - 1, x + ArrowSize), r.bottom() - 1),
		QPointF(r.right() - 1, r.bottom() - 1),
		QPointF(r.right() - 1, r.top() + 1),
	};

	if (selected()) {
		p.setPen(highlight_pen());
		p.setBrush(Qt::transparent);
		p.drawPolygon(points, countof(points));
	}

	p.setPen(Qt::transparent);
	p.setBrush(hover ? color_.lighter() : color_);
	p.drawPolygon(points, countof(points));

	p.setPen(color_.lighter());
	p.setBrush(Qt::transparent);
	p.drawPolygon(highlight_points, countof(highlight_points));

	p.setPen(color_.darker());
	p.setBrush(Qt::transparent);
	p.drawPolygon(points, countof(points));

	p.setPen(select_text_color(color_));
	p.drawText(r, Qt::AlignCenter | Qt::AlignVCenter, get_text());
}

void TimeMarker::paint_fore(QPainter &p, ViewItemPaintParams &pp)
{
	if (!enabled())
		return;

	const float x = get_x();
	p.setPen(color_.darker());
	p.drawLine(QPointF(x, pp.top()), QPointF(x, pp.bottom()));
}

pv::widgets::Popup* TimeMarker::create_popup(QWidget *parent)
{
	using pv::widgets::Popup;

	Popup *const popup = new Popup(parent);
	popup->set_position(parent->mapToGlobal(
		drag_point(parent->rect())), Popup::Bottom);

	QFormLayout *const form = new QFormLayout(popup);
	popup->setLayout(form);

	value_widget_ = new pv::widgets::TimestampSpinBox(parent);
	value_widget_->setValue(view_.ruler()->get_ruler_time_from_absolute_time(time_));

	connect(value_widget_, SIGNAL(valueChanged(const pv::util::Timestamp&)),
		this, SLOT(on_value_changed(const pv::util::Timestamp&)));

	form->addRow(tr("Time"), value_widget_);

	return popup;
}

void TimeMarker::on_value_changed(const pv::util::Timestamp& value)
{
	if (!updating_value_widget_)
		set_time(view_.ruler()->get_absolute_time_from_ruler_time(value));
}

} // namespace trace
} // namespace views
} // namespace pv
