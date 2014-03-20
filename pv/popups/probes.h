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

#ifndef PULSEVIEW_PV_POPUPS_PROBES_H
#define PULSEVIEW_PV_POPUPS_PROBES_H

#include <map>
#include <vector>

#include <boost/shared_ptr.hpp>

#include <QFormLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSignalMapper>

#include <pv/widgets/popup.h>

struct sr_channel_group;

class QCheckBox;
class QGridLayout;

namespace pv {

class SigSession;

namespace prop {
namespace binding {
class DeviceOptions;
}
}

namespace view {
class Signal;
}

namespace popups {

class Probes : public pv::widgets::Popup
{
	Q_OBJECT

public:
	Probes(SigSession &_session, QWidget *parent);

private:
	void set_all_probes(bool set);

	void populate_group(const sr_channel_group *group,
		const std::vector< boost::shared_ptr<pv::view::Signal> > sigs);

	QGridLayout* create_channel_group_grid(
		const std::vector< boost::shared_ptr<pv::view::Signal> > sigs);

private:
	void showEvent(QShowEvent *e);

private slots:
	void on_probe_checked(QWidget *widget);

	void enable_all_probes();
	void disable_all_probes();

private:
	pv::SigSession &_session;

	QFormLayout _layout;

	bool _updating_probes;

	std::vector< boost::shared_ptr<pv::prop::binding::DeviceOptions> >
		 _group_bindings;
	std::map< QCheckBox*, boost::shared_ptr<pv::view::Signal> >
		_check_box_signal_map;

	QHBoxLayout _buttons_bar;
	QPushButton _enable_all_probes;
	QPushButton _disable_all_probes;

	QSignalMapper _check_box_mapper;
};

} // popups
} // pv

#endif // PULSEVIEW_PV_POPUPS_PROBES_H
