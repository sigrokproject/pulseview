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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PULSEVIEW_PV_DEVICEMANAGER_HPP
#define PULSEVIEW_PV_DEVICEMANAGER_HPP

#include <list>
#include <map>
#include <memory>
#include <string>

using std::list;
using std::map;
using std::shared_ptr;
using std::string;

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
	DeviceManager(shared_ptr<sigrok::Context> context);

	~DeviceManager() = default;

	const shared_ptr<sigrok::Context>& context() const;

	shared_ptr<sigrok::Context> context();

	const list< shared_ptr<devices::HardwareDevice> >& devices() const;

	list< shared_ptr<devices::HardwareDevice> > driver_scan(
		shared_ptr<sigrok::Driver> driver,
		map<const sigrok::ConfigKey *, Glib::VariantBase> drvopts);

	const map<string, string> get_device_info(
		const shared_ptr<devices::Device> device);

	const shared_ptr<devices::HardwareDevice> find_device_from_info(
		const map<string, string> search_info);

private:
	bool compare_devices(shared_ptr<devices::Device> a,
		shared_ptr<devices::Device> b);

protected:
	shared_ptr<sigrok::Context> context_;
	list< shared_ptr<devices::HardwareDevice> > devices_;
};

} // namespace pv

#endif // PULSEVIEW_PV_DEVICEMANAGER_HPP
