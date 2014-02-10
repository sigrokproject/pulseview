/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2014 Joel Holdsworth <joel@airwebreathe.org.uk>
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
#include <sstream>

#include <QDebug>

#include <libsigrok/libsigrok.h>

#include "devinst.h"

using std::ostringstream;
using std::string;

namespace pv {

DevInst::DevInst(sr_dev_inst *sdi) :
	_sdi(sdi)
{
	assert(_sdi);
}

sr_dev_inst* DevInst::dev_inst() const
{
	return _sdi;
}

string DevInst::format_device_title() const
{
	ostringstream s;

	assert(_sdi);

	if (_sdi->vendor && _sdi->vendor[0]) {
		s << _sdi->vendor;
		if ((_sdi->model && _sdi->model[0]) ||
			(_sdi->version && _sdi->version[0]))
			s << ' ';
	}

	if (_sdi->model && _sdi->model[0]) {
		s << _sdi->model;
		if (_sdi->version && _sdi->version[0])
			s << ' ';
	}

	if (_sdi->version && _sdi->version[0])
		s << _sdi->version;

	return s.str();
}

GVariant* DevInst::get_config(const sr_probe_group *group, int key)
{
	GVariant *data = NULL;
	if (sr_config_get(_sdi->driver, _sdi, group, key, &data) != SR_OK)
		return NULL;
	return data;
}

bool DevInst::set_config(const sr_probe_group *group, int key, GVariant *data)
{
	if(sr_config_set(_sdi, group, key, data) == SR_OK) {
		config_changed();
		return true;
	}
	return false;
}

GVariant* DevInst::list_config(const sr_probe_group *group, int key)
{
	GVariant *data = NULL;
	if (sr_config_list(_sdi->driver, _sdi, group, key, &data) != SR_OK)
		return NULL;
	return data;
}

void DevInst::enable_probe(const sr_probe *probe, bool enable)
{
	for (const GSList *p = _sdi->probes; p; p = p->next)
		if (probe == p->data) {
			const_cast<sr_probe*>(probe)->enabled = enable;
			return;
		}

	// Probe was not found in the device
	assert(0);
}

} // pv
