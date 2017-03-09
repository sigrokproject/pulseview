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

#include "sweeptimingwidget.hpp"

#include <cstdlib>
#include <vector>
#include <cassert>

#include <extdef.h>

using std::abs;
using std::vector;

namespace pv {
namespace widgets {

SweepTimingWidget::SweepTimingWidget(const char *suffix,
	QWidget *parent) :
	QWidget(parent),
	suffix_(suffix),
	layout_(this),
	value_(this),
	list_(this),
	value_type_(None)
{
	setContentsMargins(0, 0, 0, 0);

	value_.setDecimals(0);
	value_.setSuffix(QString::fromUtf8(suffix));

	connect(&list_, SIGNAL(currentIndexChanged(int)),
		this, SIGNAL(value_changed()));
	connect(&value_, SIGNAL(editingFinished()),
		this, SIGNAL(value_changed()));

	setLayout(&layout_);
	layout_.setMargin(0);
	layout_.addWidget(&list_);
	layout_.addWidget(&value_);

	show_none();
}

void SweepTimingWidget::show_none()
{
	value_type_ = None;
	value_.hide();
	list_.hide();
}

void SweepTimingWidget::show_min_max_step(uint64_t min, uint64_t max,
	uint64_t step)
{
	assert(max > min);
	assert(step > 0);

	value_type_ = MinMaxStep;

	value_.setRange(min, max);
	value_.setSingleStep(step);

	value_.show();
	list_.hide();
}

void SweepTimingWidget::show_list(const uint64_t *vals, size_t count)
{
	value_type_ = List;

	list_.clear();
	for (size_t i = 0; i < count; i++) {
		char *const s = sr_si_string_u64(vals[i], suffix_);
		list_.addItem(QString::fromUtf8(s),
			qVariantFromValue(vals[i]));
		g_free(s);
	}

	value_.hide();
	list_.show();
}

void SweepTimingWidget::show_125_list(uint64_t min, uint64_t max)
{
	assert(max > min);

	// Create a 1-2-5-10 list of entries.
	const unsigned int FineScales[] = {1, 2, 5};
	uint64_t value, decade;
	unsigned int fine;
	vector<uint64_t> values;

	// Compute the starting decade
	for (decade = 1; decade * 10 <= min; decade *= 10);

	// Compute the first entry
	for (fine = 0; fine < countof(FineScales); fine++)
		if (FineScales[fine] * decade >= min)
			break;

	assert(fine < countof(FineScales));

	// Add the minimum entry if it's not on the 1-2-5 progression
	if (min != FineScales[fine] * decade)
		values.push_back(min);

	while ((value = FineScales[fine] * decade) < max) {
		values.push_back(value);
		if (++fine >= countof(FineScales))
			fine = 0, decade *= 10;
	}

	// Add the max value
	values.push_back(max);

	// Make a C array, and give it to the sweep timing widget
	uint64_t *const values_array = new uint64_t[values.size()];
	copy(values.begin(), values.end(), values_array);
	show_list(values_array, values.size());
	delete[] values_array;
}

uint64_t SweepTimingWidget::value() const
{
	switch(value_type_) {
	case None:
		return 0;

	case MinMaxStep:
		return (uint64_t)value_.value();

	case List:
	{
		const int index = list_.currentIndex();
		return (index >= 0) ? list_.itemData(
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
	value_.setValue(value);

	int best_match = list_.count() - 1;
	int64_t best_variance = INT64_MAX;

	for (int i = 0; i < list_.count(); i++) {
		const int64_t this_variance = abs(
			(int64_t)value - list_.itemData(i).value<int64_t>());
		if (this_variance < best_variance) {
			best_variance = this_variance;
			best_match = i;
		}
	}

	list_.setCurrentIndex(best_match);
}

} // widgets
} // pv
