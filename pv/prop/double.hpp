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

#ifndef PULSEVIEW_PV_PROP_DOUBLE_HPP
#define PULSEVIEW_PV_PROP_DOUBLE_HPP

#include <utility>

#include <boost/optional.hpp>

#include "property.hpp"

class QDoubleSpinBox;

namespace pv {
namespace prop {

class Double : public Property
{
	Q_OBJECT

public:
	Double(QString name, int decimals, QString suffix,
		boost::optional< std::pair<double, double> > range,
		boost::optional<double> step,
		Getter getter,
		Setter setter);

	virtual ~Double() = default;

	QWidget* get_widget(QWidget *parent, bool auto_commit);

	void commit();

private Q_SLOTS:
	void on_value_changed(double);

private:
	const int decimals_;
	const QString suffix_;
	const boost::optional< std::pair<double, double> > range_;
	const boost::optional<double> step_;

	QDoubleSpinBox *spin_box_;
};

} // prop
} // pv

#endif // PULSEVIEW_PV_PROP_DOUBLE_HPP
