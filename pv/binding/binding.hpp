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

#ifndef PULSEVIEW_PV_BINDING_BINDING_HPP
#define PULSEVIEW_PV_BINDING_BINDING_HPP

#include <glib.h>
// Suppress warnings due to use of deprecated std::auto_ptr<> by glibmm.
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
#include <glibmm.h>
G_GNUC_END_IGNORE_DEPRECATIONS

#include <vector>
#include <memory>

#include <QString>

class QFormLayout;
class QWidget;

namespace pv {

namespace prop {
class Property;
}

namespace binding {

class Binding
{
public:
	const std::vector< std::shared_ptr<prop::Property> >& properties();

	void commit();

	void add_properties_to_form(QFormLayout *layout,
		bool auto_commit = false) const;

	QWidget* get_property_form(QWidget *parent,
		bool auto_commit = false) const;

	static QString print_gvariant(Glib::VariantBase gvar);

protected:
	std::vector< std::shared_ptr<prop::Property> > properties_;
};

} // binding
} // pv

#endif // PULSEVIEW_PV_BINDING_BINDING_HPP
