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

#ifndef PULSEVIEW_PV_PROP_INT_H
#define PULSEVIEW_PV_PROP_INT_H

#include <utility>

#include <boost/optional.hpp>

#include "property.h"

class QSpinBox;

namespace pv {
namespace prop {

class Int : public Property
{
public:
	Int(QString name, QString suffix,
		boost::optional< std::pair<int64_t, int64_t> > range,
		Getter getter, Setter setter);

	virtual ~Int();

	QWidget* get_widget(QWidget *parent);

	void commit();

private:
	const QString _suffix;
	const boost::optional< std::pair<int64_t, int64_t> > _range;

	QSpinBox *_spin_box;
};

} // prop
} // pv

#endif // PULSEVIEW_PV_PROP_INT_H
