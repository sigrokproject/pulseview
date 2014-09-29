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

#ifndef PULSEVIEW_PV_DEVICE_DEVINST_H
#define PULSEVIEW_PV_DEVICE_DEVINST_H

#include <map>
#include <memory>
#include <string>

#include <QObject>

#include <glib.h>

#include <stdint.h>

struct sr_dev_inst;
struct sr_channel;
struct sr_channel_group;

#include <pv/sigsession.h>

namespace pv {

namespace device {

class DevInst : public QObject
{
	Q_OBJECT

protected:
	DevInst();

public:
	virtual sr_dev_inst* dev_inst() const = 0;

	virtual void use(SigSession *owner) throw(QString);

	virtual void release();

	SigSession* owner() const;

	virtual std::string format_device_title() const = 0;

	virtual std::map<std::string, std::string> get_device_info() const = 0;

	GVariant* get_config(const sr_channel_group *group, int key);

	bool set_config(const sr_channel_group *group, int key, GVariant *data);

	GVariant* list_config(const sr_channel_group *group, int key);

	void enable_channel(const sr_channel *channel, bool enable = true);

	/**
	 * @brief Gets the sample limit from the driver.
	 *
	 * @return The returned sample limit from the driver, or 0 if the
	 * 	sample limit could not be read.
	 */
	uint64_t get_sample_limit();

	virtual bool is_trigger_enabled() const;

public:
	virtual void start();

	virtual void run();

Q_SIGNALS:
	void config_changed();

protected:
	SigSession *_owner;
};

} // device
} // pv

#endif // PULSEVIEW_PV_DEVICE_DEVINST_H
