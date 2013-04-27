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

#include <cassert>
#include <cstring>
#include <stdexcept>
#include <string>

#include <libsigrok/libsigrok.h>

using namespace std;

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

const std::list<sr_dev_inst*>& DeviceManager::devices() const
{
	return _devices;
}

list<sr_dev_inst*> DeviceManager::driver_scan(
	struct sr_dev_driver *const driver, GSList *const drvopts)
{
	list<sr_dev_inst*> driver_devices;

	assert(driver);

	// Remove any device instances from this driver from the device
	// list. They will not be valid after the scan.
	list<sr_dev_inst*>::iterator i = _devices.begin();
	while (i != _devices.end()) {
		if ((*i)->driver == driver)
			i = _devices.erase(i);
		else
			i++;
	}

	// Clear all the old device instances from this driver
	sr_dev_clear(driver);

	// Do the scan
	GSList *const devices = sr_driver_scan(driver, drvopts);
	for (GSList *l = devices; l; l = l->next)
		driver_devices.push_back((sr_dev_inst*)l->data);
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

bool DeviceManager::compare_devices(const sr_dev_inst *const a,
	const sr_dev_inst *const b)
{
	assert(a);
	assert(b);

	const int vendor_cmp = strcasecmp(a->vendor, b->vendor);
	if(vendor_cmp < 0)
		return true;
	else if(vendor_cmp > 0)
		return false;

	const int model_cmp = strcasecmp(a->model, b->model);
	if(model_cmp < 0)
		return true;
	else if(model_cmp > 0)
		return false;

	return strcasecmp(a->version, b->version) < 0;
}

} // namespace pv
