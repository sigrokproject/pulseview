/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#ifndef PULSEVIEW_PV_PROP_BINDING_DEVICEOPTIONS_H
#define PULSEVIEW_PV_PROP_BINDING_DEVICEOPTIONS_H

#include <boost/function.hpp>
#include <boost/optional.hpp>

#include <QString>

#include "binding.h"

#include <glib.h>

struct sr_dev_inst;
struct sr_channel_group;

namespace pv {

namespace device {
class DevInst;
}

namespace prop {
namespace binding {

class DeviceOptions : public Binding
{
public:
	DeviceOptions(std::shared_ptr<pv::device::DevInst> dev_inst,
		const sr_channel_group *group = NULL);

private:
	void bind_bool(const QString &name, int key);
	void bind_enum(const QString &name, int key,
		GVariant *const gvar_list,
		boost::function<QString (GVariant*)> printer = print_gvariant);
	void bind_int(const QString &name, int key, QString suffix,
		boost::optional< std::pair<int64_t, int64_t> > range);

	static QString print_timebase(GVariant *const gvar);
	static QString print_vdiv(GVariant *const gvar);
	static QString print_voltage_threshold(GVariant *const gvar);

protected:
	std::shared_ptr<device::DevInst> _dev_inst;
	const sr_channel_group *const _group;
};

} // binding
} // prop
} // pv

#endif // PULSEVIEW_PV_PROP_BINDING_DEVICEOPTIONS_H
