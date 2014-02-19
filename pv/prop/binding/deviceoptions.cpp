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

#include <boost/bind.hpp>

#include <stdint.h>

#include "deviceoptions.h"

#include <pv/device/devinst.h>
#include <pv/prop/bool.h>
#include <pv/prop/double.h>
#include <pv/prop/enum.h>
#include <pv/prop/int.h>

#include <libsigrok/libsigrok.h>

using boost::bind;
using boost::function;
using boost::optional;
using boost::shared_ptr;
using std::make_pair;
using std::pair;
using std::string;
using std::vector;

namespace pv {
namespace prop {
namespace binding {

DeviceOptions::DeviceOptions(shared_ptr<pv::device::DevInst> dev_inst,
	const sr_probe_group *group) :
	_dev_inst(dev_inst),
	_group(group)
{
	assert(dev_inst);

	GVariant *gvar_opts;
	gsize num_opts;

	if (!(gvar_opts = dev_inst->list_config(group, SR_CONF_DEVICE_OPTIONS)))
		/* Driver supports no device instance options. */
		return;

	const int *const options = (const int32_t *)g_variant_get_fixed_array(
		gvar_opts, &num_opts, sizeof(int32_t));
	for (unsigned int i = 0; i < num_opts; i++) {
		const struct sr_config_info *const info =
			sr_config_info_get(options[i]);

		if (!info)
			continue;

		const int key = info->key;
		GVariant *const gvar_list = dev_inst->list_config(group, key);

		const QString name = QString::fromUtf8(info->name);

		switch(key)
		{
		case SR_CONF_SAMPLERATE:
			// Sample rate values are not bound because they are shown
			// in the SamplingBar
			break;

		case SR_CONF_CAPTURE_RATIO:
			bind_int(name, key, "%", pair<int64_t, int64_t>(0, 100));
			break;

		case SR_CONF_PATTERN_MODE:
		case SR_CONF_BUFFERSIZE:
		case SR_CONF_TRIGGER_SOURCE:
		case SR_CONF_TRIGGER_SLOPE:
		case SR_CONF_FILTER:
		case SR_CONF_COUPLING:
		case SR_CONF_CLOCK_EDGE:
			bind_enum(name, key, gvar_list);
			break;

		case SR_CONF_EXTERNAL_CLOCK:
		case SR_CONF_RLE:
			bind_bool(name, key);
			break;

		case SR_CONF_TIMEBASE:
			bind_enum(name, key, gvar_list, print_timebase);
			break;

		case SR_CONF_VDIV:
			bind_enum(name, key, gvar_list, print_vdiv);
			break;

		case SR_CONF_VOLTAGE_THRESHOLD:
			bind_enum(name, key, gvar_list, print_voltage_threshold);
			break;
		}

		if (gvar_list)
			g_variant_unref(gvar_list);
	}
	g_variant_unref(gvar_opts);
}

void DeviceOptions::bind_bool(const QString &name, int key)
{
	assert(_dev_inst);
	_properties.push_back(shared_ptr<Property>(new Bool(name,
		bind(&device::DevInst::get_config, _dev_inst, _group, key),
		bind(&device::DevInst::set_config, _dev_inst,
			_group, key, _1))));
}

void DeviceOptions::bind_enum(const QString &name, int key,
	GVariant *const gvar_list, function<QString (GVariant*)> printer)
{
	GVariant *gvar;
	GVariantIter iter;
	vector< pair<GVariant*, QString> > values;

	assert(_dev_inst);
	assert(gvar_list);

	g_variant_iter_init (&iter, gvar_list);
	while ((gvar = g_variant_iter_next_value (&iter)))
		values.push_back(make_pair(gvar, printer(gvar)));

	_properties.push_back(shared_ptr<Property>(new Enum(name, values,
		bind(&device::DevInst::get_config, _dev_inst, _group, key),
		bind(&device::DevInst::set_config, _dev_inst,
			_group, key, _1))));
}

void DeviceOptions::bind_int(const QString &name, int key, QString suffix,
	optional< std::pair<int64_t, int64_t> > range)
{
	assert(_dev_inst);

	_properties.push_back(shared_ptr<Property>(new Int(name, suffix, range,
		bind(&device::DevInst::get_config, _dev_inst, _group, key),
		bind(&device::DevInst::set_config, _dev_inst, _group, key, _1))));
}

QString DeviceOptions::print_gvariant(GVariant *const gvar)
{
	QString s;

	if (g_variant_is_of_type(gvar, G_VARIANT_TYPE("s")))
		s = QString::fromUtf8(g_variant_get_string(gvar, NULL));
	else
	{
		gchar *const text = g_variant_print(gvar, FALSE);
		s = QString::fromUtf8(text);
		g_free(text);
	}

	return s;
}

QString DeviceOptions::print_timebase(GVariant *const gvar)
{
	uint64_t p, q;
	g_variant_get(gvar, "(tt)", &p, &q);
	return QString::fromUtf8(sr_period_string(p * q));
}

QString DeviceOptions::print_vdiv(GVariant *const gvar)
{
	uint64_t p, q;
	g_variant_get(gvar, "(tt)", &p, &q);
	return QString::fromUtf8(sr_voltage_string(p, q));
}

QString DeviceOptions::print_voltage_threshold(GVariant *const gvar)
{
	gdouble lo, hi;
	char buf[64];
	g_variant_get(gvar, "(dd)", &lo, &hi);
	snprintf(buf, sizeof(buf), "L<%.1fV H>%.1fV", lo, hi);
	return QString::fromUtf8(buf);
}

} // binding
} // prop
} // pv

