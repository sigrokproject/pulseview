/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2014 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#ifndef PULSEVIEW_PV_DEVINST_H
#define PULSEVIEW_PV_DEVINST_H

#include <string>

#include <boost/shared_ptr.hpp>

#include <QObject>

#include <glib.h>

struct sr_dev_inst;
struct sr_probe_group;

namespace pv {

class DevInst : public QObject
{
	Q_OBJECT

public:
	DevInst(sr_dev_inst *sdi);

	sr_dev_inst* dev_inst() const;

	std::string format_device_title() const;

	GVariant* get_config(const sr_probe_group *group, int key);

	bool set_config(const sr_probe_group *group, int key, GVariant *data);

	GVariant* list_config(const sr_probe_group *group, int key);

signals:
	void config_changed();

private:
	sr_dev_inst *const _sdi;
};

} // pv

#endif // PULSEVIEW_PV_DEVINST_H
