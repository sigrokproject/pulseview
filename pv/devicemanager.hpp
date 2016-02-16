/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2013 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#ifndef PULSEVIEW_PV_DEVICEMANAGER_HPP
#define PULSEVIEW_PV_DEVICEMANAGER_HPP

#include <list>
#include <map>
#include <memory>
#include <string>

namespace Glib {
class VariantBase;
}

namespace sigrok {
class ConfigKey;
class Context;
class Driver;
}

namespace pv {

namespace devices {
class Device;
class HardwareDevice;
}

class Session;

class DeviceManager
{
public:
	DeviceManager(std::shared_ptr<sigrok::Context> context);

	~DeviceManager() = default;

	const std::shared_ptr<sigrok::Context>& context() const;

	std::shared_ptr<sigrok::Context> context();

	const std::list< std::shared_ptr<devices::HardwareDevice> >&
		devices() const;

	std::list< std::shared_ptr<devices::HardwareDevice> > driver_scan(
		std::shared_ptr<sigrok::Driver> driver,
		std::map<const sigrok::ConfigKey *, Glib::VariantBase> drvopts);

	const std::map<std::string, std::string> get_device_info(
		const std::shared_ptr<devices::Device> device);

	const std::shared_ptr<devices::HardwareDevice> find_device_from_info(
		const std::map<std::string, std::string> search_info);

private:
	bool compare_devices(std::shared_ptr<devices::Device> a,
		std::shared_ptr<devices::Device> b);

protected:
	std::shared_ptr<sigrok::Context> context_;
	std::list< std::shared_ptr<devices::HardwareDevice> > devices_;
};

} // namespace pv

#endif // PULSEVIEW_PV_DEVICEMANAGER_HPP
