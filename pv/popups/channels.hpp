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

#ifndef PULSEVIEW_PV_POPUPS_CHANNELS_HPP
#define PULSEVIEW_PV_POPUPS_CHANNELS_HPP

#include <map>
#include <memory>
#include <vector>

#include <QFormLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSignalMapper>

#include <pv/widgets/popup.hpp>

class QCheckBox;
class QGridLayout;

namespace sigrok {
	class ChannelGroup;
}

namespace pv {

class Session;

namespace binding {
class Device;
}

namespace data {
class SignalBase;
}

namespace view {
class Signal;
}

namespace popups {

class Channels : public pv::widgets::Popup
{
	Q_OBJECT

public:
	Channels(Session &session, QWidget *parent);

private:
	void set_all_channels(bool set);

	void populate_group(std::shared_ptr<sigrok::ChannelGroup> group,
		const std::vector< std::shared_ptr<pv::data::SignalBase> > sigs);

	QGridLayout* create_channel_group_grid(
		const std::vector< std::shared_ptr<pv::data::SignalBase> > sigs);

private:
	void showEvent(QShowEvent *event);

private Q_SLOTS:
	void on_channel_checked(QWidget *widget);

	void enable_all_channels();
	void disable_all_channels();

private:
	pv::Session &session_;

	QFormLayout layout_;

	bool updating_channels_;

	std::vector< std::shared_ptr<pv::binding::Device> >
		 group_bindings_;
	std::map< QCheckBox*, std::shared_ptr<pv::data::SignalBase> >
		check_box_signal_map_;

	QHBoxLayout buttons_bar_;
	QPushButton enable_all_channels_;
	QPushButton disable_all_channels_;

	QSignalMapper check_box_mapper_;
};

} // popups
} // pv

#endif // PULSEVIEW_PV_POPUPS_CHANNELS_HPP
