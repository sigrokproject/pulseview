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

#include <sstream>

#include <libsigrok/libsigrok.h>

#include "device.h"

using std::ostringstream;
using std::string;

namespace pv {
namespace device {

Device::Device(sr_dev_inst *sdi) :
	_sdi(sdi)
{
	assert(_sdi);
}

sr_dev_inst* Device::dev_inst() const
{
	return _sdi;
}

std::string Device::format_device_title() const
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

bool Device::is_trigger_enabled() const
{
	assert(_sdi);
	for (const GSList *l = _sdi->probes; l; l = l->next) {
		const sr_probe *const p = (const sr_probe *)l->data;
		assert(p);
		if (p->trigger && p->trigger[0] != '\0')
			return true;
	}
	return false;
}

} // device
} // pv
