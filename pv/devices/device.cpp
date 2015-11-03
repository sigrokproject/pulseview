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

#include <cassert>

#include <libsigrokcxx/libsigrokcxx.hpp>

#include "device.hpp"

using std::map;
using std::set;

using sigrok::ConfigKey;
using sigrok::Capability;
using sigrok::Error;

using Glib::VariantBase;
using Glib::Variant;

namespace pv {
namespace devices {

Device::Device()
{
}

Device::~Device()
{
	if (session_)
		session_->remove_datafeed_callbacks();
}

std::shared_ptr<sigrok::Session> Device::session() const
{
	return session_;
}

std::shared_ptr<sigrok::Device> Device::device() const
{
	return device_;
}

template
uint64_t Device::read_config(const sigrok::ConfigKey*,
	const uint64_t);

template<typename T>
T Device::read_config(const ConfigKey *key, const T default_value)
{
	assert(key);

	if (!device_)
		return default_value;

	if (!device_->config_check(key, Capability::GET))
		return default_value;

	return VariantBase::cast_dynamic<Glib::Variant<guint64>>(
		device_->config_get(ConfigKey::SAMPLERATE)).get();
}

void Device::start()
{
	assert(session_);
	session_->start();
}

void Device::run()
{
	assert(device_);
	assert(session_);
	session_->run();
}

void Device::stop()
{
	assert(session_);
	session_->stop();
}

} // namespace devices
} // namespace pv
