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

#ifndef PULSEVIEW_PV_DEVICEMANAGER_H
#define PULSEVIEW_PV_DEVICEMANAGER_H

#include <glib.h>

#include <list>
#include <map>
#include <string>

#include <boost/shared_ptr.hpp>

struct sr_context;
struct sr_dev_driver;

namespace pv {

class SigSession;

namespace device {
class DevInst;
}

class DeviceManager
{
public:
	DeviceManager(struct sr_context *sr_ctx);

	~DeviceManager();

	const std::list< boost::shared_ptr<pv::device::DevInst> >&
		devices() const;

	void use_device(boost::shared_ptr<pv::device::DevInst> dev_inst,
		SigSession *owner);

	void release_device(boost::shared_ptr<pv::device::DevInst> dev_inst);

	std::list< boost::shared_ptr<pv::device::DevInst> > driver_scan(
		struct sr_dev_driver *const driver,
		GSList *const drvopts = NULL);

private:
	void init_drivers();

	void release_devices();

	void scan_all_drivers();

	void release_driver(struct sr_dev_driver *const driver);

	static bool compare_devices(boost::shared_ptr<device::DevInst> a,
		boost::shared_ptr<device::DevInst> b);

private:
	struct sr_context *const _sr_ctx;
	std::list< boost::shared_ptr<pv::device::DevInst> > _devices;
	std::map< boost::shared_ptr<pv::device::DevInst>, pv::SigSession*>
		_used_devices;
};

} // namespace pv

#endif // PULSEVIEW_PV_DEVICEMANAGER_H
