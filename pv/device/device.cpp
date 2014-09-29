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

#include <libsigrok/libsigrok.h>

#include "device.h"

using std::list;
using std::make_pair;
using std::map;
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

void Device::use(SigSession *owner) throw(QString)
{
	DevInst::use(owner);

	sr_session_new(&SigSession::_sr_session);

	assert(_sdi);
	sr_dev_open(_sdi);
	if (sr_session_dev_add(SigSession::_sr_session, _sdi) != SR_OK)
		throw QString(tr("Failed to use device."));
}

void Device::release()
{
	if (_owner) {
		DevInst::release();
		sr_session_destroy(SigSession::_sr_session);
	}

	sr_dev_close(_sdi);
}

std::string Device::format_device_title() const
{
	ostringstream s;

	assert(_sdi);

	if (_sdi->vendor && _sdi->vendor[0])
		s << _sdi->vendor << " ";

	if (_sdi->model && _sdi->model[0])
		s << _sdi->model << " ";

	if (_sdi->version && _sdi->version[0])
		s << _sdi->version << " ";

	// Show connection string only if no serial number is present.
	if (_sdi->serial_num && _sdi->serial_num[0])
		s << "(" << _sdi->serial_num << ") ";
	else if (_sdi->connection_id && _sdi->connection_id[0])
		s << "[" << _sdi->connection_id << "] ";

	// Remove trailing space.
	s.seekp(-1, std::ios_base::end);
	s << std::ends;

	return s.str();
}

map<string, string> Device::get_device_info() const
{
	map<string, string> result;

	assert(_sdi);

	if (_sdi->vendor && _sdi->vendor[0])
		result.insert(make_pair("vendor", _sdi->vendor));

	if (_sdi->model && _sdi->model[0])
		result.insert(make_pair("model", _sdi->model));

	if (_sdi->version && _sdi->version[0])
		result.insert(make_pair("version", _sdi->version));

	if (_sdi->serial_num && _sdi->serial_num[0])
		result.insert(make_pair("serial_num", _sdi->serial_num));

	if (_sdi->connection_id && _sdi->connection_id[0])
		result.insert(make_pair("connection_id", _sdi->connection_id));

	return result;
}

} // device
} // pv
