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

#include <boost/foreach.hpp>

#include <QFormLayout>

#include <pv/prop/property.h>

#include "binding.h"

using namespace boost;

namespace pv {
namespace prop {
namespace binding {

const std::vector< boost::shared_ptr<Property> >& Binding::properties()
{
	return _properties;
}

void Binding::commit()
{
	BOOST_FOREACH(shared_ptr<pv::prop::Property> p, _properties) {
		assert(p);
		p->commit();
	}
}

void Binding::add_properties_to_form(QFormLayout *layout) const
{
	assert(layout);

	BOOST_FOREACH(shared_ptr<pv::prop::Property> p, _properties)
	{
		assert(p);
		const QString label = p->labeled_widget() ? QString() : p->name();
		layout->addRow(label, p->get_widget(layout->parentWidget()));
	}
}

QWidget* Binding::get_property_form(QWidget *parent) const
{
	QWidget *const form = new QWidget(parent);
	QFormLayout *const layout = new QFormLayout(form);
	form->setLayout(layout);
	add_properties_to_form(layout);
	return form;
}

} // binding
} // prop
} // pv
