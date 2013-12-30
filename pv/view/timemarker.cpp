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
	_view(view),
	_colour(colour),
	_time(time),
	_value_action(NULL),
	_value_widget(NULL),
	_updating_value_widget(false)
{
}

double TimeMarker::time() const
{
	return _time;
}

float TimeMarker::get_x() const
{
	return (_time - _view.offset()) / _view.scale();
}

void TimeMarker::set_time(double time)
{
	_time = time;

	if (_value_widget) {
		_updating_value_widget = true;
		_value_widget->setValue(time);
		_updating_value_widget = false;
	}

	time_changed();
}

void TimeMarker::paint(QPainter &p, const QRect &rect)
{
	const float x = get_x();
	p.setPen(_colour);
	p.drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
}

pv::widgets::Popup* TimeMarker::create_popup(QWidget *parent)
{
	using pv::widgets::Popup;

	Popup *const popup = new Popup(parent);
	QFormLayout *const form = new QFormLayout(popup);
	popup->setLayout(form);

	_value_widget = new QDoubleSpinBox(parent);
	_value_widget->setDecimals(9);
	_value_widget->setSuffix("s");
	_value_widget->setSingleStep(1e-6);
	_value_widget->setValue(_time);

	connect(_value_widget, SIGNAL(valueChanged(double)),
		this, SLOT(on_value_changed(double)));

	form->addRow(tr("Time"), _value_widget);

	return popup;
}

void TimeMarker::on_value_changed(double value)
{
	if (!_updating_value_widget) {
		_time = value;
		time_changed();
	}
}

} // namespace view
} // namespace pv
