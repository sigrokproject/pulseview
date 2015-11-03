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

#ifndef PULSEVIEW_PV_BINDING_DEVICE_HPP
#define PULSEVIEW_PV_BINDING_DEVICE_HPP

#include <boost/optional.hpp>

#include <QObject>
#include <QString>

#include "binding.hpp"

#include <pv/prop/property.hpp>

#include <libsigrokcxx/libsigrokcxx.hpp>

namespace pv {

namespace binding {

class Device : public QObject, public Binding
{
	Q_OBJECT

public:
	Device(std::shared_ptr<sigrok::Configurable> configurable);

Q_SIGNALS:
	void config_changed();

private:
	void bind_bool(const QString &name,
		prop::Property::Getter getter, prop::Property::Setter setter);
	void bind_enum(const QString &name,
		const sigrok::ConfigKey *key,
		std::set<const sigrok::Capability *> capabilities,
		prop::Property::Getter getter, prop::Property::Setter setter,
		std::function<QString (Glib::VariantBase)> printer = print_gvariant);
	void bind_int(const QString &name, QString suffix,
		boost::optional< std::pair<int64_t, int64_t> > range,
		prop::Property::Getter getter, prop::Property::Setter setter);

	static QString print_timebase(Glib::VariantBase gvar);
	static QString print_vdiv(Glib::VariantBase gvar);
	static QString print_voltage_threshold(Glib::VariantBase gvar);

protected:
	std::shared_ptr<sigrok::Configurable> configurable_;
};

} // binding
} // pv

#endif // PULSEVIEW_PV_BINDING_DEVICE_HPP
