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

#include "timemarker.h"

#include "view.h"

#include <QFormLayout>
#include <QPainter>

#include <pv/widgets/popup.h>

namespace pv {
namespace view {

TimeMarker::TimeMarker(View &view, const QColor &colour, double time) :
	view_(view),
	colour_(colour),
	time_(time),
	value_action_(NULL),
	value_widget_(NULL),
	updating_value_widget_(false)
{
}

double TimeMarker::time() const
{
	return time_;
}

float TimeMarker::get_x() const
{
	return (time_ - view_.offset()) / view_.scale();
}

QPoint TimeMarker::point() const
{
	return QPoint(get_x(), 0);
}

void TimeMarker::set_time(double time)
{
	time_ = time;

	if (value_widget_) {
		updating_value_widget_ = true;
		value_widget_->setValue(time);
		updating_value_widget_ = false;
	}

	time_changed();
}

void TimeMarker::paint(QPainter &p, const QRect &rect)
{
	const float x = get_x();
	p.setPen(colour_);
	p.drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
}

pv::widgets::Popup* TimeMarker::create_popup(QWidget *parent)
{
	using pv::widgets::Popup;

	Popup *const popup = new Popup(parent);
	QFormLayout *const form = new QFormLayout(popup);
	popup->setLayout(form);

	value_widget_ = new QDoubleSpinBox(parent);
	value_widget_->setDecimals(9);
	value_widget_->setSuffix("s");
	value_widget_->setSingleStep(1e-6);
	value_widget_->setValue(time_);

	connect(value_widget_, SIGNAL(valueChanged(double)),
		this, SLOT(on_value_changed(double)));

	form->addRow(tr("Time"), value_widget_);

	return popup;
}

void TimeMarker::on_value_changed(double value)
{
	if (!updating_value_widget_) {
		time_ = value;
		time_changed();
	}
}

} // namespace view
} // namespace pv
