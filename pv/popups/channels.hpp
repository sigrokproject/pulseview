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

#ifndef PULSEVIEW_PV_POPUPS_CHANNELS_HPP
#define PULSEVIEW_PV_POPUPS_CHANNELS_HPP

#include <functional>
#include <map>
#include <memory>
#include <vector>

#include <QCheckBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalMapper>

#include <pv/widgets/popup.hpp>

using std::function;
using std::map;
using std::shared_ptr;
using std::vector;

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

	void enable_channels_conditionally(
		function<bool (const shared_ptr<data::SignalBase>)> cond_func);
	void disable_channels_conditionally(
		function<bool (const shared_ptr<data::SignalBase>)> cond_func);

	void populate_group(shared_ptr<sigrok::ChannelGroup> group,
		const vector< shared_ptr<pv::data::SignalBase> > sigs);

	QGridLayout* create_channel_group_grid(
		const vector< shared_ptr<pv::data::SignalBase> > sigs);

	void showEvent(QShowEvent *event);

private Q_SLOTS:
	void on_channel_checked(QWidget *widget);

	void enable_all_channels();
	void disable_all_channels();
	void enable_all_logic_channels();
	void disable_all_logic_channels();
	void enable_all_analog_channels();
	void disable_all_analog_channels();
	void enable_all_named_channels();
	void disable_all_unnamed_channels();
	void enable_all_changing_channels();
	void disable_all_non_changing_channels();

private:
	pv::Session &session_;

	QFormLayout layout_;

	bool updating_channels_;

	vector< shared_ptr<pv::binding::Device> > group_bindings_;
	map< QCheckBox*, shared_ptr<pv::data::SignalBase> >
		check_box_signal_map_;
	map< shared_ptr<sigrok::ChannelGroup>, QLabel*> group_label_map_;

	QGridLayout filter_buttons_bar_;
	QPushButton enable_all_channels_, disable_all_channels_;
	QPushButton enable_all_logic_channels_, disable_all_logic_channels_;
	QPushButton enable_all_analog_channels_, disable_all_analog_channels_;
	QPushButton enable_all_named_channels_, disable_all_unnamed_channels_;
	QPushButton enable_all_changing_channels_, disable_all_non_changing_channels_;

	QSignalMapper check_box_mapper_;
};

}  // namespace popups
}  // namespace pv

#endif // PULSEVIEW_PV_POPUPS_CHANNELS_HPP
