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

#include <libsigrokcxx/libsigrokcxx.hpp>

#include <boost/filesystem.hpp>

#include "sessionfile.hpp"

namespace pv {
namespace devices {

SessionFile::SessionFile(const std::shared_ptr<sigrok::Context> context,
	const std::string &file_name) :
	File(file_name),
	context_(context)
{
}

void SessionFile::open()
{
	if (session_)
		close();

	session_ = context_->load_session(file_name_);
	device_ = session_->devices()[0];
}

void SessionFile::close()
{
	if (session_)
		session_->remove_devices();
}

} // namespace devices
} // namespace pv
