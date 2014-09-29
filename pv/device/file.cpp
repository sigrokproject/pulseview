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

#include "file.h"
#include "sessionfile.h"

#include <boost/filesystem.hpp>

#include <libsigrok/libsigrok.h>

using std::make_pair;
using std::map;
using std::string;

namespace pv {
namespace device {

File::File(const std::string path) :
	_path(path)
{
}

std::string File::format_device_title() const
{
	return boost::filesystem::path(_path).filename().string();
}

map<string, string> File::get_device_info() const
{
	map<string, string> result;

	result.insert(make_pair("vendor", "sigrok"));
	result.insert(make_pair("model", "file"));
	result.insert(make_pair("connection_id",
			boost::filesystem::path(_path).filename().string()));

	return result;
}

File* File::create(const string &name)
{
	if (sr_session_load(name.c_str(), &SigSession::_sr_session) == SR_OK) {
		GSList *devlist = NULL;
		sr_session_dev_list(SigSession::_sr_session, &devlist);
		sr_session_destroy(SigSession::_sr_session);

		if (devlist) {
			sr_dev_inst *const sdi = (sr_dev_inst*)devlist->data;
			g_slist_free(devlist);
			if (sdi) {
				sr_dev_close(sdi);
				sr_dev_clear(sdi->driver);
				return new SessionFile(name);
			}
		}
	}

	return NULL;
}

} // device
} // pv
