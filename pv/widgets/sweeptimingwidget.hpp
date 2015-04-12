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

#ifndef PULSEVIEW_PV_WIDGETS_SWEEPTIMINGWIDGET_HPP
#define PULSEVIEW_PV_WIDGETS_SWEEPTIMINGWIDGET_HPP

#include <libsigrok/libsigrok.h>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QWidget>

namespace pv {
namespace widgets {

class SweepTimingWidget : public QWidget
{
	Q_OBJECT

private:
	enum ValueType
	{
		None,
		MinMaxStep,
		List
	};

public:
	SweepTimingWidget(const char *suffix, QWidget *parent = nullptr);

	void show_none();
	void show_min_max_step(uint64_t min, uint64_t max, uint64_t step);
	void show_list(const uint64_t *vals, size_t count);
	void show_125_list(uint64_t min, uint64_t max);

	uint64_t value() const;
	void set_value(uint64_t value);

Q_SIGNALS:
	void value_changed();

private:
	const char *const suffix_;

	QHBoxLayout layout_;

	QDoubleSpinBox value_;
	QComboBox list_;

	ValueType value_type_;
};

} // widgets
} // pv

#endif // PULSEVIEW_PV_WIDGETS_SWEEPTIMINGWIDGET_HPP
