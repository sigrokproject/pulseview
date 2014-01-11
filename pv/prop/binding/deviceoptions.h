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

#include <libsigrok/libsigrok.h>

#include "binding.h"

namespace pv {
namespace prop {
namespace binding {

class DeviceOptions : public Binding
{
public:
	DeviceOptions(const sr_dev_inst *sdi,
		const sr_probe_group *group = NULL);

private:

	static GVariant* config_getter(
		const sr_dev_inst *sdi, const sr_probe_group *group, int key);
	static void config_setter(
		const sr_dev_inst *sdi, const sr_probe_group *group, int key,
		GVariant* value);

	void bind_bool(const QString &name, int key);
	void bind_enum(const QString &name, int key,
		GVariant *const gvar_list,
		boost::function<QString (GVariant*)> printer = print_gvariant);
	void bind_int(const QString &name, int key, QString suffix,
		boost::optional< std::pair<int64_t, int64_t> > range);

	static QString print_gvariant(GVariant *const gvar);

	static QString print_timebase(GVariant *const gvar);
	static QString print_vdiv(GVariant *const gvar);
	static QString print_voltage_threshold(GVariant *const gvar);

protected:
	const sr_dev_inst *const _sdi;
	const sr_probe_group *const _group;
};

} // binding
} // prop
} // pv

#endif // PULSEVIEW_PV_PROP_BINDING_DEVICEOPTIONS_H
