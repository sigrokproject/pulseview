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

#include "devicemanager.h"
#include "device/device.h"
#include "sigsession.h"

#include <cassert>
#include <stdexcept>
#include <string>

#include <libsigrok/libsigrok.h>

using std::list;
using std::map;
using std::runtime_error;
using std::shared_ptr;
using std::string;

namespace pv {

DeviceManager::DeviceManager(struct sr_context *sr_ctx) :
	_sr_ctx(sr_ctx)
{
	init_drivers();
	scan_all_drivers();
}

DeviceManager::~DeviceManager()
{
	release_devices();
}

const list< shared_ptr<pv::device::Device> >& DeviceManager::devices() const
{
	return _devices;
}

list< shared_ptr<device::Device> > DeviceManager::driver_scan(
	struct sr_dev_driver *const driver, GSList *const drvopts)
{
	list< shared_ptr<device::Device> > driver_devices;

	assert(driver);

	// Remove any device instances from this driver from the device
	// list. They will not be valid after the scan.
	auto i = _devices.begin();
	while (i != _devices.end()) {
		if ((*i)->dev_inst()->driver == driver)
			i = _devices.erase(i);
		else
			i++;
	}

	// Release this driver and all it's attached devices
	release_driver(driver);

	// Do the scan
	GSList *const devices = sr_driver_scan(driver, drvopts);
	for (GSList *l = devices; l; l = l->next)
		driver_devices.push_back(shared_ptr<device::Device>(
			new device::Device((sr_dev_inst*)l->data)));
	g_slist_free(devices);
	driver_devices.sort(compare_devices);

	// Add the scanned devices to the main list
	_devices.insert(_devices.end(), driver_devices.begin(),
		driver_devices.end());
	_devices.sort(compare_devices);

	return driver_devices;
}

const shared_ptr<device::Device> DeviceManager::find_device_from_info(
	const map<string, string> search_info)
{
	shared_ptr<device::Device> last_resort_dev;
	map<string, string> dev_info;

	last_resort_dev = NULL;

	for (shared_ptr<device::Device> dev : _devices) {
		assert(dev);
		dev_info = dev->get_device_info();

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

void DeviceManager::init_drivers()
{
	// Initialise all libsigrok drivers
	sr_dev_driver **const drivers = sr_driver_list();
	for (sr_dev_driver **driver = drivers; *driver; driver++) {
		if (sr_driver_init(_sr_ctx, *driver) != SR_OK) {
			throw runtime_error(
				string("Failed to initialize driver ") +
				string((*driver)->name));
		}
	}
}

void DeviceManager::release_devices()
{
	// Release all the used devices
	for (shared_ptr<device::Device> dev : _devices) {
		assert(dev);
		dev->release();
	}

	// Clear all the drivers
	sr_dev_driver **const drivers = sr_driver_list();
	for (sr_dev_driver **driver = drivers; *driver; driver++)
		sr_dev_clear(*driver);
}

void DeviceManager::scan_all_drivers()
{
	// Scan all drivers for all devices.
	struct sr_dev_driver **const drivers = sr_driver_list();
	for (struct sr_dev_driver **driver = drivers; *driver; driver++)
		driver_scan(*driver);
}

void DeviceManager::release_driver(struct sr_dev_driver *const driver)
{
	for (shared_ptr<device::Device> dev : _devices) {
		assert(dev);
		if(dev->dev_inst()->driver == driver)
			dev->release();
	}

	// Clear all the old device instances from this driver
	sr_dev_clear(driver);
}

bool DeviceManager::compare_devices(shared_ptr<device::Device> a,
	shared_ptr<device::Device> b)
{
	assert(a);
	assert(b);
	return a->format_device_title().compare(b->format_device_title()) < 0;
}

} // namespace pv
