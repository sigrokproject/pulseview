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

#include <cassert>

#include <QFormLayout>

#include <pv/prop/property.hpp>

#include "binding.hpp"

using std::shared_ptr;

namespace pv {
namespace binding {

const std::vector< std::shared_ptr<prop::Property> >& Binding::properties()
{
	return properties_;
}

void Binding::commit()
{
	for (shared_ptr<pv::prop::Property> p : properties_) {
		assert(p);
		p->commit();
	}
}

void Binding::add_properties_to_form(QFormLayout *layout,
	bool auto_commit) const
{
	assert(layout);

	for (shared_ptr<pv::prop::Property> p : properties_)
	{
		assert(p);

		QWidget *const widget = p->get_widget(layout->parentWidget(),
			auto_commit);
		if (p->labeled_widget())
			layout->addRow(widget);
		else
			layout->addRow(p->name(), widget);
	}
}

QWidget* Binding::get_property_form(QWidget *parent,
	bool auto_commit) const
{
	QWidget *const form = new QWidget(parent);
	QFormLayout *const layout = new QFormLayout(form);
	form->setLayout(layout);
	add_properties_to_form(layout, auto_commit);
	return form;
}

QString Binding::print_gvariant(Glib::VariantBase gvar)
{
	QString s;

	if (!gvar.gobj())
		s = QString::fromStdString("(null)");
	else if (gvar.is_of_type(Glib::VariantType("s")))
		s = QString::fromStdString(
			Glib::VariantBase::cast_dynamic<Glib::Variant<std::string>>(
				gvar).get());
	else
		s = QString::fromStdString(gvar.print());

	return s;
}

} // binding
} // pv
