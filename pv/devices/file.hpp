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

#ifndef PULSEVIEW_PV_DEVICES_FILE_HPP
#define PULSEVIEW_PV_DEVICES_FILE_HPP

#include <string>

#include "device.hpp"

namespace pv {
namespace devices {

class File : public Device
{
protected:
	File(const std::string &file_name);

public:
	/**
	 * Builds the full name. It only contains all the fields.
	 */
	std::string full_name() const;

	/**
	 * Builds the display name. It only contains fields as required.
	 */
	std::string display_name(const DeviceManager&) const;

protected:
	const std::string file_name_;
};

} // namespace devices
} // namespace pv

#endif // PULSEVIEW_PV_DEVICES_FILE_HPP
