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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "sweeptimingwidget.h"

#include <assert.h>

namespace pv {
namespace widgets {

SweepTimingWidget::SweepTimingWidget(const char *suffix,
	QWidget *parent) :
	QWidget(parent),
	_layout(this),
	_value(this),
	_list(this),
	_value_type(None)
{
	setContentsMargins(0, 0, 0, 0);

	_value.setDecimals(0);
	_value.setSuffix(QString::fromUtf8(suffix));

	connect(&_list, SIGNAL(currentIndexChanged(int)),
		this, SIGNAL(value_changed()));
	connect(&_value, SIGNAL(editingFinished()),
		this, SIGNAL(value_changed()));

	setLayout(&_layout);
	_layout.setMargin(0);
	_layout.addWidget(&_list);
	_layout.addWidget(&_value);

	show_none();
}

void SweepTimingWidget::show_none()
{
	_value_type = None;
	_value.hide();
	_list.hide();
}

void SweepTimingWidget::show_min_max_step(uint64_t min, uint64_t max,
	uint64_t step)
{
	_value_type = MinMaxStep;

	_value.setRange(min, max);
	_value.setSingleStep(step);

	_value.show();
	_list.hide();
}

void SweepTimingWidget::show_list(const uint64_t *vals, size_t count)
{
	_value_type = List;

	_list.clear();
	for (size_t i = 0; i < count; i++)
	{
		char *const s = sr_samplerate_string(vals[i]);
		_list.addItem(QString::fromUtf8(s),
			qVariantFromValue(vals[i]));
		g_free(s);
	}

	_value.hide();
	_list.show();
}

uint64_t SweepTimingWidget::value() const
{
	switch(_value_type)
	{
	case None:
		return 0;

	case MinMaxStep:
		return (uint64_t)_value.value();

	case List:
	{
		const int index = _list.currentIndex();
		return (index >= 0) ? _list.itemData(
			index).value<uint64_t>() : 0;
	}

	default:
		// Unexpected value type
		assert(0);
		return 0;
	}
}

void SweepTimingWidget::set_value(uint64_t value)
{
	_value.setValue(value);

	for (int i = 0; i < _list.count(); i++)
		if (value == _list.itemData(i).value<uint64_t>())
			_list.setCurrentIndex(i);
}

} // widgets
} // pv
