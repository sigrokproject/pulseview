/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2015 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#ifndef PULSEVIEW_PV_BINDING_INPUTOUTPUT_HPP
#define PULSEVIEW_PV_BINDING_INPUTOUTPUT_HPP

#include <map>
#include <memory>
#include <string>

#include "binding.hpp"

#include <pv/prop/property.hpp>

using std::map;
using std::shared_ptr;
using std::string;
using std::vector;

namespace sigrok {
class Option;
}

namespace pv {
namespace binding {

/**
 * A binding of glibmm variants for sigrok input and output options.
 */
class InputOutput : public Binding
{
public:
	/**
	 * Constructs a new @c InputOutput binding.
	 * @param options the map of options to use as a template.
	 */
	InputOutput(const map<string, shared_ptr<sigrok::Option>> &options);

	/**
	 * Gets the map of selected options.
	 * @return the options.
	 */
	const map<string, Glib::VariantBase>& options() const;

private:
	/**
	 * A helper function to bind an option list to and enum property.
	 * @param name the name of the property.
	 * @param values the list of values.
	 * @param getter the getter that will read the values out of the map.
	 * @param setter the setter that will set the values into the map.
	 */
	shared_ptr<prop::Property> bind_enum(const QString &name,
		const vector<Glib::VariantBase> &values,
		prop::Property::Getter getter, prop::Property::Setter setter);

private:
	/**
	 * The current map of options.
	 */
	map<string, Glib::VariantBase> options_;
};

} // binding
} // pv

#endif // PULSEVIEW_PV_BINDING_INPUTOUTPUT_H
