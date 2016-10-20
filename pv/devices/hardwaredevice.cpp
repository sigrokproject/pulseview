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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <boost/algorithm/string/join.hpp>

#include <QString>

#include <libsigrokcxx/libsigrokcxx.hpp>

#include <pv/devicemanager.hpp>

#include "hardwaredevice.hpp"

using std::dynamic_pointer_cast;
using std::shared_ptr;
using std::static_pointer_cast;
using std::string;
using std::vector;

using boost::algorithm::join;

using sigrok::HardwareDevice;

namespace pv {
namespace devices {

HardwareDevice::HardwareDevice(const std::shared_ptr<sigrok::Context> &context,
	std::shared_ptr<sigrok::HardwareDevice> device) :
	context_(context),
	device_open_(false)
{
	device_ = device;
}

HardwareDevice::~HardwareDevice()
{
	close();
}

string HardwareDevice::full_name() const
{
	vector<string> parts = {device_->vendor(), device_->model(),
		device_->version(), device_->serial_number()};
	if (device_->connection_id().length() > 0)
		parts.push_back("(" + device_->connection_id() + ")");
	return join(parts, " ");
}

shared_ptr<sigrok::HardwareDevice> HardwareDevice::hardware_device() const
{
	return static_pointer_cast<sigrok::HardwareDevice>(device_);
}

string HardwareDevice::display_name(
	const DeviceManager &device_manager) const
{
	const auto hw_dev = hardware_device();

	// If we can find another device with the same model/vendor then
	// we have at least two such devices and need to distinguish them.
	const auto &devices = device_manager.devices();
	const bool multiple_dev = hw_dev && any_of(
		devices.begin(), devices.end(),
		[&](shared_ptr<devices::HardwareDevice> dev) {
			return dev->hardware_device()->vendor() ==
					hw_dev->vendor() &&
				dev->hardware_device()->model() ==
					hw_dev->model() &&
				dev->device_ != device_;
		});

	vector<string> parts = {device_->vendor(), device_->model()};

	if (multiple_dev) {
		parts.push_back(device_->version());
		parts.push_back(device_->serial_number());

		if ((device_->serial_number().length() == 0) &&
			(device_->connection_id().length() > 0))
			parts.push_back("(" + device_->connection_id() + ")");
	}

	return join(parts, " ");
}

void HardwareDevice::open()
{
	if (device_open_)
		close();

	try {
		device_->open();
	} catch (const sigrok::Error &e) {
		throw QString(e.what());
	}

	device_open_ = true;

	// Set up the session
	session_ = context_->create_session();
	session_->add_device(device_);
}

void HardwareDevice::close()
{
	if (device_open_)
		device_->close();

	if (session_)
		session_->remove_devices();

	device_open_ = false;
}

} // namespace devices
} // namespace pv
