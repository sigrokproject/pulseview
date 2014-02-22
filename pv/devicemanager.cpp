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

using boost::shared_ptr;
using std::list;
using std::map;
using std::ostringstream;
using std::runtime_error;
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

void DeviceManager::use_device(shared_ptr<device::Device> dev_inst,
	SigSession *owner)
{
	assert(dev_inst);
	assert(owner);

	_used_devices[dev_inst] = owner;

	sr_dev_open(dev_inst->dev_inst());
}

void DeviceManager::release_device(shared_ptr<device::Device> dev_inst)
{
	assert(dev_inst);

	// Notify the owner, and remove the device from the used device list
	map< shared_ptr<device::Device>, pv::SigSession*>::const_iterator
		iter = _used_devices.find(dev_inst);
	assert(iter != _used_devices.end());

	(*iter).second->release_device(dev_inst);
	_used_devices.erase(dev_inst);
}

list< shared_ptr<device::Device> > DeviceManager::driver_scan(
	struct sr_dev_driver *const driver, GSList *const drvopts)
{
	list< shared_ptr<device::Device> > driver_devices;

	assert(driver);

	// Remove any device instances from this driver from the device
	// list. They will not be valid after the scan.
	list< shared_ptr<device::Device> >::iterator i = _devices.begin();
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
	for (map<shared_ptr<device::Device>, SigSession*>::iterator i =
		_used_devices.begin(); i != _used_devices.end(); i++)
		release_device((*i).first);

	_used_devices.clear();

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
	assert(driver);
	for (map<shared_ptr<device::Device>, SigSession*>::iterator i =
		_used_devices.begin(); i != _used_devices.end(); i++)
		if((*i).first->dev_inst()->driver == driver)
		{
			// Notify the current owner of the device
			(*i).second->release_device((*i).first);

			// Remove it from the used device list
			_used_devices.erase(i);

			// Close the device instance
			sr_dev_close((*i).first->dev_inst());
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
