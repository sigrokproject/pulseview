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

#include "devicemanager.hpp"
#include "session.hpp"

#include <cassert>
#include <stdexcept>
#include <sstream>
#include <string>
#include <vector>

#include <libsigrok/libsigrok.hpp>

#include <boost/algorithm/string/join.hpp>
#include <boost/filesystem.hpp>

using boost::algorithm::join;

using std::dynamic_pointer_cast;
using std::list;
using std::map;
using std::remove_if;
using std::runtime_error;
using std::shared_ptr;
using std::string;
using std::vector;

using Glib::VariantBase;

using sigrok::ConfigKey;
using sigrok::Context;
using sigrok::Driver;
using sigrok::Device;
using sigrok::HardwareDevice;
using sigrok::SessionDevice;

namespace pv {

DeviceManager::DeviceManager(shared_ptr<Context> context) :
	context_(context)
{
	for (auto entry : context->drivers())
		driver_scan(entry.second, map<const ConfigKey *, VariantBase>());
}

DeviceManager::~DeviceManager()
{
}

shared_ptr<Context> DeviceManager::context()
{
	return context_;
}

const list< shared_ptr<HardwareDevice> >& DeviceManager::devices() const
{
	return devices_;
}

list< shared_ptr<HardwareDevice> > DeviceManager::driver_scan(
	shared_ptr<Driver> driver, map<const ConfigKey *, VariantBase> drvopts)
{
	list< shared_ptr<HardwareDevice> > driver_devices;

	assert(driver);

	// Remove any device instances from this driver from the device
	// list. They will not be valid after the scan.
	devices_.remove_if([&](shared_ptr<HardwareDevice> device) {
		return device->driver() == driver; });

	// Do the scan
	auto devices = driver->scan(drvopts);
	driver_devices.insert(driver_devices.end(), devices.begin(), devices.end());

	// Add the scanned devices to the main list, set display names and sort.
	devices_.insert(devices_.end(), driver_devices.begin(),
		driver_devices.end());

	for (shared_ptr<Device> device : devices_)
		build_display_name(device);

	devices_.sort([&](shared_ptr<Device> a, shared_ptr<Device> b)
		{ return compare_devices(a, b); });

	// As the display names depend on the complete devices_ list,
	// we need to recompute them. However, there is no need to
	// recomute all names of the devices_ list since only the
	// devices that use the given driver can be affected.
	for (shared_ptr<Device> device : driver_devices)
		build_display_name(device);

	driver_devices.sort([&](shared_ptr<Device> a, shared_ptr<Device> b)
		{ return compare_devices(a, b); });

	return driver_devices;
}

const map<string, string> DeviceManager::get_device_info(
	shared_ptr<Device> device)
{
	map<string, string> result;

	assert(device);

	if (device->vendor().length() > 0)
		result["vendor"] = device->vendor();
	if (device->model().length() > 0)
		result["model"] = device->model();
	if (device->version().length() > 0)
		result["version"] = device->version();
	if (device->serial_number().length() > 0)
		result["serial_num"] = device->serial_number();
	if (device->connection_id().length() > 0)
		result["connection_id"] = device->connection_id();

	return result;
}

const shared_ptr<HardwareDevice> DeviceManager::find_device_from_info(
	const map<string, string> search_info)
{
	shared_ptr<HardwareDevice> last_resort_dev;
	map<string, string> dev_info;

	last_resort_dev = NULL;

	for (shared_ptr<HardwareDevice> dev : devices_) {
		assert(dev);
		dev_info = get_device_info(dev);

		// If present, vendor and model always have to match.
		if (dev_info.count("vendor") > 0 && search_info.count("vendor") > 0)
			if (dev_info.at("vendor") != search_info.at("vendor")) continue;

		if (dev_info.count("model") > 0 && search_info.count("model") > 0)
			if (dev_info.at("model") != search_info.at("model")) continue;

		// Most unique match: vendor/model/serial_num (but don't match a S/N of 0)
		if ((dev_info.count("serial_num") > 0) && (dev_info.at("serial_num") != "0")
				&& search_info.count("serial_num") > 0)
			if (dev_info.at("serial_num") == search_info.at("serial_num") &&
					dev_info.at("serial_num") != "0")
				return dev;

		// Second best match: vendor/model/connection_id
		if (dev_info.count("connection_id") > 0 &&
			search_info.count("connection_id") > 0)
			if (dev_info.at("connection_id") == search_info.at("connection_id"))
				return dev;

		// Last resort: vendor/model/version
		if (dev_info.count("version") > 0 &&
			search_info.count("version") > 0)
			if (dev_info.at("version") == search_info.at("version") &&
					dev_info.at("version") != "0")
				return dev;

		// For this device, we merely have a vendor/model match.
		last_resort_dev = dev;
	}

	// If there wasn't even a vendor/model/version match, we end up here.
	// This is usually the case for devices with only vendor/model data.
	// The selected device may be wrong with multiple such devices attached
	// but it is the best we can do at this point. After all, there may be
	// only one such device and we do want to select it in this case.
	return last_resort_dev;
}

void DeviceManager::build_display_name(shared_ptr<Device> device)
{
	auto session_device = dynamic_pointer_cast<SessionDevice>(device);
	auto hardware_device = dynamic_pointer_cast<HardwareDevice>(device);

	if (session_device) {
		full_names_[device] = display_names_[device] =
			boost::filesystem::path(
			session_device->parent()->filename()).filename().string();
		return;
	}

	// First, build the device's full name. It always contains all
	// possible information.
	vector<string> parts = {device->vendor(), device->model(),
		device->version(), device->serial_number()};

	if (device->connection_id().length() > 0)
		parts.push_back("("+device->connection_id()+")");

	full_names_[device] = join(parts, " ");

	// Next, build the display name. It only contains fields as required.
	bool multiple_dev = false;

	// If we can find another device with the same model/vendor then
	// we have at least two such devices and need to distinguish them.
	if (hardware_device)
		multiple_dev = any_of(devices_.begin(), devices_.end(),
			[&](shared_ptr<HardwareDevice> dev) {
			return (dev->vendor() == hardware_device->vendor() &&
			dev->model() == hardware_device->model()) &&
			dev != hardware_device;
			} );

	parts = {device->vendor(), device->model()};

	if (multiple_dev) {
		parts.push_back(device->version());
		parts.push_back(device->serial_number());

		if ((device->serial_number().length() == 0) &&
			(device->connection_id().length() > 0))
			parts.push_back("("+device->connection_id()+")");
	}

	display_names_[device] = join(parts, " ");
}

const std::string DeviceManager::get_display_name(std::shared_ptr<sigrok::Device> dev)
{
	return display_names_[dev];
}

const std::string DeviceManager::get_full_name(std::shared_ptr<sigrok::Device> dev)
{
	return full_names_[dev];
}

void DeviceManager::update_display_name(std::shared_ptr<sigrok::Device> dev)
{
	build_display_name(dev);
}

bool DeviceManager::compare_devices(shared_ptr<Device> a,
	shared_ptr<Device> b)
{
	assert(a);
	assert(b);

	return display_names_[a].compare(display_names_[b]) < 0;
}

} // namespace pv
