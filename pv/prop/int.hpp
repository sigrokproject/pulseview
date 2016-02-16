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

#ifndef PULSEVIEW_PV_PROP_INT_HPP
#define PULSEVIEW_PV_PROP_INT_HPP

#include <utility>

#include <boost/optional.hpp>

#include "property.hpp"

class QSpinBox;

namespace pv {
namespace prop {

class Int : public Property
{
	Q_OBJECT;

public:
	Int(QString name, QString suffix,
		boost::optional< std::pair<int64_t, int64_t> > range,
		Getter getter, Setter setter);

	virtual ~Int() = default;

	QWidget* get_widget(QWidget *parent, bool auto_commit);

	void commit();

private Q_SLOTS:
	void on_value_changed(int);

private:
	const QString suffix_;
	const boost::optional< std::pair<int64_t, int64_t> > range_;

	Glib::VariantBase value_;
	QSpinBox *spin_box_;
};

} // prop
} // pv

#endif // PULSEVIEW_PV_PROP_INT_HPP
