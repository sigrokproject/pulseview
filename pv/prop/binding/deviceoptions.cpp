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

#include <QDebug>
#include <QObject>

#include "deviceoptions.h"

#include <pv/prop/double.h>
#include <pv/prop/enum.h>

using namespace boost;
using namespace std;

namespace pv {
namespace prop {
namespace binding {

DeviceOptions::DeviceOptions(struct sr_dev_inst *sdi) :
	_sdi(sdi)
{
	const int *options;

	if ((sr_config_list(sdi->driver, SR_CONF_DEVICE_OPTIONS,
		(const void **)&options, sdi) != SR_OK) || !options)
		/* Driver supports no device instance options. */
		return;

	for (int cap = 0; options[cap]; cap++) {
		const struct sr_config_info *const info =
			sr_config_info_get(options[cap]);

		if (!info)
			continue;

		switch(info->key)
		{
		case SR_CONF_SAMPLERATE:
			bind_samplerate(info);
			break;

		case SR_CONF_PATTERN_MODE:
			bind_stropt(info, SR_CONF_PATTERN_MODE);
			break;

		case SR_CONF_BUFFERSIZE:
			bind_buffer_size(info);
			break;

		case SR_CONF_TIMEBASE:
			bind_time_base(info);
			break;

		case SR_CONF_TRIGGER_SOURCE:
			bind_stropt(info, SR_CONF_TRIGGER_SOURCE);
			break;

		case SR_CONF_FILTER:
			bind_stropt(info, SR_CONF_FILTER);
			break;

		case SR_CONF_VDIV:
			bind_vdiv(info);
			break;

		case SR_CONF_COUPLING:
			bind_stropt(info, SR_CONF_FILTER);
			break;
		}
	}
}

void DeviceOptions::expose_enum(const struct sr_config_info *info,
	const vector< pair<const void*, QString> > &values, int key)
{
	_properties.push_back(shared_ptr<Property>(
		new Enum(QString(info->name), values,
			bind(enum_getter, _sdi, key),
			bind(sr_config_set, _sdi, key, _1))));
}

void DeviceOptions::bind_stropt(
	const struct sr_config_info *info, int key)
{
	const char **stropts;
	if (sr_config_list(_sdi->driver, key,
		(const void **)&stropts, _sdi) != SR_OK)
		return;

	vector< pair<const void*, QString> > values;
	for (int i = 0; stropts[i]; i++)
		values.push_back(make_pair(stropts[i], stropts[i]));

	expose_enum(info, values, key);
}

void DeviceOptions::bind_samplerate(const struct sr_config_info *info)
{
	const struct sr_samplerates *samplerates;

	if (sr_config_list(_sdi->driver, SR_CONF_SAMPLERATE,
		(const void **)&samplerates, _sdi) != SR_OK)
		return;

	if (samplerates->step) {
		_properties.push_back(shared_ptr<Property>(
			new Double(QString(info->name),
				0, QObject::tr("Hz"),
				make_pair((double)samplerates->low,
					(double)samplerates->high),
				(double)samplerates->step,
				bind(samplerate_value_getter, _sdi),
				bind(samplerate_value_setter, _sdi, _1))));
	} else {
		vector< pair<const void*, QString> > values;
		for (const uint64_t *rate = samplerates->list;
			*rate; rate++)
			values.push_back(make_pair(
				(const void*)rate,
				QString(sr_samplerate_string(*rate))));

		_properties.push_back(shared_ptr<Property>(
			new Enum(QString(info->name), values,
				bind(samplerate_list_getter, _sdi),
				bind(samplerate_list_setter, _sdi, _1))));
	}
}

void DeviceOptions::bind_buffer_size(const struct sr_config_info *info)
{
	const uint64_t *sizes;
	if (sr_config_list(_sdi->driver, SR_CONF_BUFFERSIZE,
			(const void **)&sizes, _sdi) != SR_OK)
		return;

	vector< pair<const void*, QString> > values;
	for (int i = 0; sizes[i]; i++)
		values.push_back(make_pair(sizes + i,
			QString("%1").arg(sizes[i])));

	expose_enum(info, values, SR_CONF_BUFFERSIZE);
}

void DeviceOptions::bind_time_base(const struct sr_config_info *info)
{
	struct sr_rational *timebases;
	if (sr_config_list(_sdi->driver, SR_CONF_TIMEBASE,
			(const void **)&timebases, _sdi) != SR_OK)
		return;

	vector< pair<const void*, QString> > values;
	for (int i = 0; timebases[i].p && timebases[i].q; i++)
		values.push_back(make_pair(timebases + i,
			QString(sr_period_string(
				timebases[i].p * timebases[i].q))));

	expose_enum(info, values, SR_CONF_TIMEBASE);
}

void DeviceOptions::bind_vdiv(const struct sr_config_info *info)
{
	struct sr_rational *vdivs;
	if (sr_config_list(_sdi->driver, SR_CONF_VDIV,
			(const void **)&vdivs, _sdi) != SR_OK)
		return;

	vector< pair<const void*, QString> > values;
	for (int i = 0; vdivs[i].p && vdivs[i].q; i++)
		values.push_back(make_pair(vdivs + i,
			QString(sr_voltage_string(vdivs + i))));

	expose_enum(info, values, SR_CONF_VDIV);
}

const void* DeviceOptions::enum_getter(
	const struct sr_dev_inst *sdi, int key)
{
	const void *data = NULL;
	if(sr_config_get(sdi->driver, key, &data, sdi) != SR_OK) {
		qDebug() <<
			"WARNING: Failed to get value of config id" << key;
		return NULL;
	}
	return data;
}

double DeviceOptions::samplerate_value_getter(
	const struct sr_dev_inst *sdi)
{
	uint64_t *samplerate = NULL;
	if(sr_config_get(sdi->driver, SR_CONF_SAMPLERATE,
		(const void**)&samplerate, sdi) != SR_OK) {
		qDebug() <<
			"WARNING: Failed to get value of sample rate";
		return 0.0;
	}
	return (double)*samplerate;
}

void DeviceOptions::samplerate_value_setter(
	struct sr_dev_inst *sdi, double value)
{
	uint64_t samplerate = value;
	if(sr_config_set(sdi, SR_CONF_SAMPLERATE,
		&samplerate) != SR_OK)
		qDebug() <<
			"WARNING: Failed to set value of sample rate";
}

const void* DeviceOptions::samplerate_list_getter(
	const struct sr_dev_inst *sdi)
{
	const struct sr_samplerates *samplerates;
	uint64_t *samplerate = NULL;

	if (sr_config_list(sdi->driver, SR_CONF_SAMPLERATE,
		(const void **)&samplerates, sdi) != SR_OK) {
		qDebug() <<
			"WARNING: Failed to get enumerate sample rates";
		return NULL;
	}

	if(sr_config_get(sdi->driver, SR_CONF_SAMPLERATE,
		(const void**)&samplerate, sdi) != SR_OK ||
		!samplerate) {
		qDebug() <<
			"WARNING: Failed to get value of sample rate";
		return NULL;
	}

	for (const uint64_t *rate = samplerates->list; *rate; rate++)
		if(*rate == *samplerate)
			return (const void*)rate;

	return NULL;
}

void DeviceOptions::samplerate_list_setter(
	struct sr_dev_inst *sdi, const void *value)
{
	if (sr_config_set(sdi, SR_CONF_SAMPLERATE,
		(uint64_t*)value) != SR_OK)
		qDebug() <<
			"WARNING: Failed to set value of sample rate";
}

} // binding
} // prop
} // pv

