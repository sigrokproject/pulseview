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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <cstdint>

#include <QDebug>

#include "device.hpp"

#include <pv/prop/bool.hpp>
#include <pv/prop/enum.hpp>
#include <pv/prop/int.hpp>

#include <libsigrokcxx/libsigrokcxx.hpp>

using boost::optional;

using std::function;
using std::pair;
using std::set;
using std::shared_ptr;
using std::string;
using std::vector;

using sigrok::Capability;
using sigrok::Configurable;
using sigrok::ConfigKey;
using sigrok::Error;

using pv::prop::Bool;
using pv::prop::Enum;
using pv::prop::Int;
using pv::prop::Property;

namespace pv {
namespace binding {

Device::Device(shared_ptr<sigrok::Configurable> configurable) :
	configurable_(configurable)
{

	auto keys = configurable->config_keys();

	for (auto key : keys) {

		auto capabilities = configurable->config_capabilities(key);

		if (!capabilities.count(Capability::GET) ||
			!capabilities.count(Capability::SET))
			continue;

		string name_str;
		try {
			name_str = key->description();
		} catch (Error& e) {
			name_str = key->name();
		}

		const QString name = QString::fromStdString(name_str);

		const Property::Getter get = [&, key]() {
			return configurable_->config_get(key); };
		const Property::Setter set = [&, key](Glib::VariantBase value) {
			configurable_->config_set(key, value);
			config_changed();
		};

		switch (key->id()) {
		case SR_CONF_SAMPLERATE:
			// Sample rate values are not bound because they are shown
			// in the MainBar
			break;

		case SR_CONF_CAPTURE_RATIO:
			bind_int(name, "", "%", pair<int64_t, int64_t>(0, 100), get, set);
			break;

		case SR_CONF_PATTERN_MODE:
		case SR_CONF_BUFFERSIZE:
		case SR_CONF_TRIGGER_SOURCE:
		case SR_CONF_TRIGGER_SLOPE:
		case SR_CONF_COUPLING:
		case SR_CONF_CLOCK_EDGE:
		case SR_CONF_DATA_SOURCE:
		case SR_CONF_EXTERNAL_CLOCK_SOURCE:
			bind_enum(name, "", key, capabilities, get, set);
			break;

		case SR_CONF_FILTER:
		case SR_CONF_EXTERNAL_CLOCK:
		case SR_CONF_RLE:
		case SR_CONF_POWER_OFF:
		case SR_CONF_AVERAGING:
			bind_bool(name, "", get, set);
			break;

		case SR_CONF_TIMEBASE:
			bind_enum(name, "", key, capabilities, get, set, print_timebase);
			break;

		case SR_CONF_VDIV:
			bind_enum(name, "", key, capabilities, get, set, print_vdiv);
			break;

		case SR_CONF_VOLTAGE_THRESHOLD:
			bind_enum(name, "", key, capabilities, get, set, print_voltage_threshold);
			break;

		case SR_CONF_PROBE_FACTOR:
			if (capabilities.count(Capability::LIST))
				bind_enum(name, "", key, capabilities, get, set, print_probe_factor);
			else
				bind_int(name, "", "", pair<int64_t, int64_t>(1, 500), get, set);
			break;

		case SR_CONF_AVG_SAMPLES:
			if (capabilities.count(Capability::LIST))
				bind_enum(name, "", key, capabilities, get, set, print_averages);
			else
				bind_int(name, "", "", pair<int64_t, int64_t>(0, INT32_MAX), get, set);
			break;

		default:
			break;
		}
	}
}

void Device::bind_bool(const QString &name, const QString &desc,
	Property::Getter getter, Property::Setter setter)
{
	assert(configurable_);
	properties_.push_back(shared_ptr<Property>(new Bool(
		name, desc, getter, setter)));
}

void Device::bind_enum(const QString &name, const QString &desc,
	const ConfigKey *key, set<const Capability *> capabilities,
	Property::Getter getter,
	Property::Setter setter, function<QString (Glib::VariantBase)> printer)
{
	assert(configurable_);

	if (!capabilities.count(Capability::LIST))
		return;

	try {
		Glib::VariantContainerBase gvar = configurable_->config_list(key);
		Glib::VariantIter iter(gvar);

		vector< pair<Glib::VariantBase, QString> > values;
		while ((iter.next_value(gvar)))
			values.emplace_back(gvar, printer(gvar));

		properties_.push_back(shared_ptr<Property>(new Enum(name, desc, values,
			getter, setter)));

	} catch (sigrok::Error& e) {
		qDebug() << "Error: Listing device key" << name << "failed!";
		return;
	}
}

void Device::bind_int(const QString &name, const QString &desc, QString suffix,
	optional< pair<int64_t, int64_t> > range,
	Property::Getter getter, Property::Setter setter)
{
	assert(configurable_);

	properties_.push_back(shared_ptr<Property>(new Int(name, desc, suffix,
		range, getter, setter)));
}

QString Device::print_timebase(Glib::VariantBase gvar)
{
	uint64_t p, q;
	g_variant_get(gvar.gobj(), "(tt)", &p, &q);
	return QString::fromUtf8(sr_period_string(p, q));
}

QString Device::print_vdiv(Glib::VariantBase gvar)
{
	uint64_t p, q;
	g_variant_get(gvar.gobj(), "(tt)", &p, &q);
	return QString::fromUtf8(sr_voltage_string(p, q));
}

QString Device::print_voltage_threshold(Glib::VariantBase gvar)
{
	gdouble lo, hi;
	g_variant_get(gvar.gobj(), "(dd)", &lo, &hi);
	return QString("L<%1V H>%2V").arg(lo, 0, 'f', 1).arg(hi, 0, 'f', 1);
}

QString Device::print_probe_factor(Glib::VariantBase gvar)
{
	uint64_t factor;
	factor = g_variant_get_uint64(gvar.gobj());
	return QString("%1x").arg(factor);
}

QString Device::print_averages(Glib::VariantBase gvar)
{
	uint64_t avg;
	avg = g_variant_get_uint64(gvar.gobj());
	return QString("%1").arg(avg);
}

}  // namespace binding
}  // namespace pv
