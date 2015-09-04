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
#include <string>

namespace sigrok {
class ConfigKey;
class Device;
class Session;
} // namespace sigrok

namespace pv {

class DeviceManager;

namespace devices {

class Device
{
protected:
	Device();

public:
	virtual ~Device();

	std::shared_ptr<sigrok::Session> session() const;

	std::shared_ptr<sigrok::Device> device() const;

	template<typename T>
	T read_config(const sigrok::ConfigKey *key, const T default_value = 0);

	/**
	 * Builds the full name. It only contains all the fields.
	 */
	virtual std::string full_name() const = 0;

	/**
	 * Builds the display name. It only contains fields as required.
	 * @param device_manager a reference to the device manager is needed
	 * so that other similarly titled devices can be detected.
	 */
	virtual std::string display_name(
		const DeviceManager &device_manager) const = 0;

	virtual void open() = 0;

	virtual void close() = 0;

	virtual void start();

	virtual void run();

	virtual void stop();

protected:
	std::shared_ptr<sigrok::Session> session_;
	std::shared_ptr<sigrok::Device> device_;
};

} // namespace devices
} // namespace pv

#endif // PULSEVIEW_PV_DEVICES_DEVICE_HPP
