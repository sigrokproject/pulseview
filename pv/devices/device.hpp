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

#ifndef PULSEVIEW_PV_DEVICES_DEVICE_HPP
#define PULSEVIEW_PV_DEVICES_DEVICE_HPP

#include <memory>


namespace sigrok {
class Device;
class Session;
} // namespace sigrok

namespace pv {
namespace devices {

class Device
{
protected:
	Device();

public:
	virtual ~Device();

	std::shared_ptr<sigrok::Session> session() const;

	std::shared_ptr<sigrok::Device> device() const;

	virtual void create() = 0;

	virtual void run();

	virtual void stop();

protected:
	std::shared_ptr<sigrok::Session> session_;
	std::shared_ptr<sigrok::Device> device_;
};

} // namespace devices
} // namespace pv

#endif // PULSEVIEW_PV_DEVICES_DEVICE_HPP
