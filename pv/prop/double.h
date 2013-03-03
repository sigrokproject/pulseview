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

#ifndef PULSEVIEW_PV_PROP_DOUBLE_H
#define PULSEVIEW_PV_PROP_DOUBLE_H

#include <utility>
#include <vector>

#include <boost/optional.hpp>

#include "property.h"

class QDoubleSpinBox;

namespace pv {
namespace prop {

class Double : public Property
{
public:
	Double(QString name, int decimals, QString suffix,
		boost::optional< std::pair<double, double> > range,
		boost::optional<double> step,
		boost::function<double ()> getter,
		boost::function<void (double)> setter);

	QWidget* get_widget(QWidget *parent);

	void commit();

private:
	const int _decimals;
	const QString _suffix;
	const boost::optional< std::pair<double, double> > _range;
	const boost::optional<double> _step;
	boost::function<double ()> _getter;
	boost::function<void (double)> _setter;

	QDoubleSpinBox *_spin_box;
};

} // prop
} // pv

#endif // PULSEVIEW_PV_PROP_DOUBLE_H
