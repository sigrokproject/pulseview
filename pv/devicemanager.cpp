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

#include "devicemanager.hpp"
#include "session.hpp"

#include <cassert>
#include <functional>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#include <libsigrokcxx/libsigrokcxx.hpp>

#include <QApplication>
#include <QObject>
#include <QProgressDialog>

#include <boost/filesystem.hpp>

#include <pv/devices/hardwaredevice.hpp>
#include <pv/util.hpp>

using std::bind;
using std::list;
using std::map;
using std::placeholders::_1;
using std::placeholders::_2;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;

using Glib::ustring;
using Glib::Variant;
using Glib::VariantBase;

using sigrok::ConfigKey;
using sigrok::Context;
using sigrok::Driver;

namespace pv {

DeviceManager::DeviceManager(shared_ptr<Context> context, std::string driver) :
	context_(context)
{
	unique_ptr<QProgressDialog> progress(new QProgressDialog("",
		QObject::tr("Cancel"), 0, context->drivers().size() + 1));
	progress->setWindowModality(Qt::WindowModal);
	progress->setMinimumDuration(1);  // To show the dialog immediately

	int entry_num = 1;

	/*
	 * Check the presence of an optional user spec for device scans.
	 * Determine the driver name and options (in generic format) when
	 * applicable.
	 */
	std::string user_name;
	vector<std::string> user_opts;
	if (!driver.empty()) {
		user_opts = pv::util::split_string(driver, ":");
		user_name = user_opts.front();
		user_opts.erase(user_opts.begin());
	}

	/*
	 * Scan for devices. No specific options apply here, this is
	 * best effort auto detection.
	 */
	for (auto entry : context->drivers()) {
		progress->setLabelText(QObject::tr("Scanning for %1...")
			.arg(QString::fromStdString(entry.first)));

		if (entry.first == user_name)
			continue;
		driver_scan(entry.second, map<const ConfigKey *, VariantBase>());

		progress->setValue(entry_num++);
		QApplication::processEvents();
		if (progress->wasCanceled())
			break;
	}

	/*
	 * Optionally run another scan with potentially more specific
	 * options when requested by the user. This is motivated by
	 * several different uses: It can find devices that are not
	 * covered by the above auto detection (UART, TCP). It can
	 * prefer one out of multiple found devices, and have this
	 * device pre-selected for new sessions upon user's request.
	 */
	user_spec_device_.reset();
	if (!driver.empty()) {
		shared_ptr<sigrok::Driver> scan_drv;
		map<const ConfigKey *, VariantBase> scan_opts;

		/*
		 * Lookup the device driver name.
		 */
		map<string, shared_ptr<Driver>> drivers = context->drivers();
		auto entry = drivers.find(user_name);
		scan_drv = (entry != drivers.end()) ? entry->second : nullptr;

		/*
		 * Convert generic string representation of options
		 * to the driver specific data types.
		 */
		if (scan_drv && !user_opts.empty()) {
			auto drv_opts = scan_drv->scan_options();
			scan_opts = drive_scan_options(user_opts, drv_opts);
		}

		/*
		 * Run another scan for the specified driver, passing
		 * user provided scan options this time.
		 */
		list< shared_ptr<devices::HardwareDevice> > found;
		if (scan_drv) {
			found = driver_scan(scan_drv, scan_opts);
			if (!found.empty())
				user_spec_device_ = found.front();
		}
	}
	progress->setValue(entry_num++);
}

const shared_ptr<sigrok::Context>& DeviceManager::context() const
{
	return context_;
}

shared_ptr<Context> DeviceManager::context()
{
	return context_;
}

const list< shared_ptr<devices::HardwareDevice> >&
DeviceManager::devices() const
{
	return devices_;
}

/**
 * Get the device that was detected with user provided scan options.
 */
shared_ptr<devices::HardwareDevice>
DeviceManager::user_spec_device() const
{
	return user_spec_device_;
}

/**
 * Convert generic options to data types that are specific to Driver::scan().
 *
 * @param[in] user_spec vector of tokenized words, string format
 * @param[in] driver_opts driver's scan options, result of Driver::scan_options()
 *
 * @return map of options suitable for Driver::scan()
 */
map<const ConfigKey *, Glib::VariantBase>
DeviceManager::drive_scan_options(vector<string> user_spec,
	set<const ConfigKey *> driver_opts)
{
	map<const ConfigKey *, Glib::VariantBase> result;

	for (auto entry : user_spec) {
		/*
		 * Split key=value specs. Accept entries without separator
		 * (for simplified boolean specifications).
		 */
		string key, val;
		size_t pos = entry.find("=");
		if (pos == std::string::npos) {
			key = entry;
			val = "";
		} else {
			key = entry.substr(0, pos);
			val = entry.substr(pos + 1);
		}

		/*
		 * Skip user specifications that are not a member of the
		 * driver's set of supported options. Have the text format
		 * input spec converted to the required driver specific type.
		 */
		const ConfigKey *cfg;
		try {
			cfg = ConfigKey::get_by_identifier(key);
			if (!cfg)
				continue;
			if (driver_opts.find(cfg) == driver_opts.end())
				continue;
		} catch (...) {
			continue;
		}
		result[cfg] = cfg->parse_string(val);
	}

	return result;
}

list< shared_ptr<devices::HardwareDevice> >
DeviceManager::driver_scan(
	shared_ptr<Driver> driver, map<const ConfigKey *, VariantBase> drvopts)
{
	list< shared_ptr<devices::HardwareDevice> > driver_devices;

	assert(driver);

	/*
	 * We currently only support devices that can deliver samples at
	 * a fixed samplerate (i.e. oscilloscopes and logic analysers).
	 *
	 * @todo Add support for non-monotonic devices (DMMs, sensors, etc).
	 */
	const auto keys = driver->config_keys();
	bool supported_device = keys.count(ConfigKey::LOGIC_ANALYZER) |
		keys.count(ConfigKey::OSCILLOSCOPE);
	if (!supported_device)
		return driver_devices;

	// Remove any device instances from this driver from the device
	// list. They will not be valid after the scan.
	devices_.remove_if([&](shared_ptr<devices::HardwareDevice> device) {
		return device->hardware_device()->driver() == driver; });

	// Do the scan
	auto devices = driver->scan(drvopts);

	// Add the scanned devices to the main list, set display names and sort.
	for (shared_ptr<sigrok::HardwareDevice> device : devices) {
		const shared_ptr<devices::HardwareDevice> d(
			new devices::HardwareDevice(context_, device));
		driver_devices.push_back(d);
	}

	devices_.insert(devices_.end(), driver_devices.begin(),
		driver_devices.end());
	devices_.sort(bind(&DeviceManager::compare_devices, this, _1, _2));
	driver_devices.sort(bind(
		&DeviceManager::compare_devices, this, _1, _2));

	return driver_devices;
}

const map<string, string> DeviceManager::get_device_info(
	shared_ptr<devices::Device> device)
{
	map<string, string> result;

	assert(device);

	const shared_ptr<sigrok::Device> sr_dev = device->device();
	if (sr_dev->vendor().length() > 0)
		result["vendor"] = sr_dev->vendor();
	if (sr_dev->model().length() > 0)
		result["model"] = sr_dev->model();
	if (sr_dev->version().length() > 0)
		result["version"] = sr_dev->version();
	if (sr_dev->serial_number().length() > 0)
		result["serial_num"] = sr_dev->serial_number();
	if (sr_dev->connection_id().length() > 0)
		result["connection_id"] = sr_dev->connection_id();

	return result;
}

const shared_ptr<devices::HardwareDevice> DeviceManager::find_device_from_info(
	const map<string, string> search_info)
{
	shared_ptr<devices::HardwareDevice> last_resort_dev;
	map<string, string> dev_info;

	for (shared_ptr<devices::HardwareDevice> dev : devices_) {
		assert(dev);
		dev_info = get_device_info(dev);

		// If present, vendor and model always have to match.
		if (dev_info.count("vendor") > 0 && search_info.count("vendor") > 0)
			if (dev_info.at("vendor") != search_info.at("vendor"))
				continue;

		if (dev_info.count("model") > 0 && search_info.count("model") > 0)
			if (dev_info.at("model") != search_info.at("model"))
				continue;

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

bool DeviceManager::compare_devices(shared_ptr<devices::Device> a,
	shared_ptr<devices::Device> b)
{
	assert(a);
	assert(b);
	return a->display_name(*this).compare(b->display_name(*this)) < 0;
}

} // namespace pv
