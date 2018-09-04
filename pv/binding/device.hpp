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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PULSEVIEW_PV_BINDING_DEVICE_HPP
#define PULSEVIEW_PV_BINDING_DEVICE_HPP

#include <functional>

#include <boost/optional.hpp>

#include <QObject>
#include <QString>

#include "binding.hpp"

#include <pv/prop/property.hpp>

#include <libsigrokcxx/libsigrokcxx.hpp>

using std::function;
using std::pair;
using std::set;
using std::shared_ptr;

namespace pv {

namespace binding {

class Device : public Binding
{
	Q_OBJECT

public:
	Device(shared_ptr<sigrok::Configurable> configurable);

Q_SIGNALS:
	void config_changed();

private:
	void bind_bool(const QString &name, const QString &desc,
		prop::Property::Getter getter, prop::Property::Setter setter);
	void bind_enum(const QString &name, const QString &desc,
		const sigrok::ConfigKey *key,
		set<const sigrok::Capability *> capabilities,
		prop::Property::Getter getter, prop::Property::Setter setter,
		function<QString (Glib::VariantBase)> printer = print_gvariant);
	void bind_int(const QString &name, const QString &desc, QString suffix,
		boost::optional< pair<int64_t, int64_t> > range,
		prop::Property::Getter getter, prop::Property::Setter setter);

	static QString print_timebase(Glib::VariantBase gvar);
	static QString print_vdiv(Glib::VariantBase gvar);
	static QString print_voltage_threshold(Glib::VariantBase gvar);
	static QString print_probe_factor(Glib::VariantBase gvar);

protected:
	shared_ptr<sigrok::Configurable> configurable_;
};

}  // namespace binding
}  // namespace pv

#endif // PULSEVIEW_PV_BINDING_DEVICE_HPP
