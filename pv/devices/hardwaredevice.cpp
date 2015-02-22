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

#include <QString>

#include <libsigrokcxx/libsigrokcxx.hpp>

#include "hardwaredevice.hpp"

using std::shared_ptr;
using std::static_pointer_cast;

namespace pv {
namespace devices {

HardwareDevice::HardwareDevice(const std::shared_ptr<sigrok::Context> &context,
	std::shared_ptr<sigrok::HardwareDevice> device) :
	context_(context) {
	device_ = device;
}

HardwareDevice::~HardwareDevice() {
	device_->close();
	if (session_)
		session_->remove_devices();
}

shared_ptr<sigrok::HardwareDevice> HardwareDevice::hardware_device() const {
	return static_pointer_cast<sigrok::HardwareDevice>(device_);
}

void HardwareDevice::create() {
	// Open the device
        try {
                device_->open();
        } catch(const sigrok::Error &e) {
                throw QString(e.what());
        }

        // Set up the session
        session_ = context_->create_session();
        session_->add_device(device_);
}

} // namespace devices
} // namespace pv
