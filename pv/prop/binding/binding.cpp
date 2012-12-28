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

#include <QFormLayout>
#include <QWidget>

#include <boost/foreach.hpp>

#include "binding.h"

#include <pv/prop/property.h>

using namespace boost;

namespace pv {
namespace prop {
namespace binding {

Binding::Binding() :
	_form(NULL)
{
}

QWidget* Binding::get_form(QWidget *parent)
{
	if(_form)
		return _form;

	_form = new QWidget(parent);
	QFormLayout *const layout = new QFormLayout(_form);
	_form->setLayout(layout);

	BOOST_FOREACH(shared_ptr<Property> p, _properties)
	{
		assert(p);
		layout->addRow(p->name(), p->get_widget(_form));
	}

	return _form;
}

void Binding::commit()
{
	BOOST_FOREACH(shared_ptr<Property> p, _properties)
	{
		assert(p);
		p->commit();
	}
}

} // binding
} // prop
} // pv
