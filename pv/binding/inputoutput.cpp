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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <cassert>
#include <iostream>
#include <utility>

#include <boost/none_t.hpp>

#include <libsigrokcxx/libsigrokcxx.hpp>

#include <pv/prop/bool.hpp>
#include <pv/prop/double.hpp>
#include <pv/prop/enum.hpp>
#include <pv/prop/int.hpp>
#include <pv/prop/string.hpp>

#include "inputoutput.hpp"

using boost::none;

using std::make_pair;
using std::map;
using std::pair;
using std::shared_ptr;
using std::string;
using std::vector;

using Glib::VariantBase;
using Glib::VariantType;

using sigrok::Option;

using pv::prop::Bool;
using pv::prop::Double;
using pv::prop::Enum;
using pv::prop::Int;
using pv::prop::Property;
using pv::prop::String;

namespace pv {
namespace binding {

InputOutput::InputOutput(
	const map<string, shared_ptr<Option>> &options)
{
	for (pair<string, shared_ptr<Option>> o : options)
	{
		const shared_ptr<Option> &opt = o.second;
		assert(opt);

		const QString name = QString::fromStdString(opt->name());
		const VariantBase def_val = opt->default_value();
		const vector<VariantBase> values = opt->values();

		options_[opt->id()] = def_val;
 
		const Property::Getter get = [&, opt]() {
			return options_[opt->id()]; };
		const Property::Setter set = [&, opt](VariantBase value) {
			options_[opt->id()] = value; };

		shared_ptr<Property> prop;

		if (!opt->values().empty())
			prop = bind_enum(name, values, get, set);
		else if (def_val.is_of_type(VariantType("b")))
			prop = shared_ptr<Property>(new Bool(name, get, set));
		else if (def_val.is_of_type(VariantType("d")))
			prop = shared_ptr<Property>(new Double(name, 2, "",
				none, none, get, set));
		else if (def_val.is_of_type(VariantType("i")) ||
			def_val.is_of_type(VariantType("t")) ||
			def_val.is_of_type(VariantType("u")))
			prop = shared_ptr<Property>(
				new Int(name, "", none, get, set));
		else if (def_val.is_of_type(VariantType("s")))
			prop = shared_ptr<Property>(
				new String(name, get, set));
		else
			continue;

		properties_.push_back(prop);
	}
}

const map<string, VariantBase>& InputOutput::options() const
{
	return options_;
}

shared_ptr<Property> InputOutput::bind_enum(
	const QString &name, const vector<VariantBase> &values,
	Property::Getter getter, Property::Setter setter)
{
	vector< pair<VariantBase, QString> > enum_vals;
	for (VariantBase var : values)
		enum_vals.push_back(make_pair(var, print_gvariant(var)));
	return shared_ptr<Property>(new Enum(name, enum_vals, getter, setter));
}

} // namespace binding
} // namespace pv
