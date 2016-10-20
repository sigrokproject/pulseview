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

#include <map>

#ifdef _WIN32
// Windows: Avoid boost/thread namespace pollution (which includes windows.h).
#define NOGDI
#define NORESOURCE
#endif
#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>

#include <QCheckBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>

#include "channels.hpp"

#include <pv/binding/device.hpp>
#include <pv/data/signalbase.hpp>
#include <pv/devices/device.hpp>
#include <pv/session.hpp>
#include <pv/view/signal.hpp>

#include <libsigrokcxx/libsigrokcxx.hpp>

using namespace Qt;

using boost::shared_lock;
using boost::shared_mutex;
using std::lock_guard;
using std::map;
using std::mutex;
using std::set;
using std::shared_ptr;
using std::unordered_set;
using std::vector;

using pv::data::SignalBase;

using sigrok::Channel;
using sigrok::ChannelGroup;
using sigrok::Device;

namespace pv {
namespace popups {

Channels::Channels(Session &session, QWidget *parent) :
	Popup(parent),
	session_(session),
	updating_channels_(false),
	enable_all_channels_(tr("Enable All"), this),
	disable_all_channels_(tr("Disable All"), this),
	check_box_mapper_(this)
{
	// Create the layout
	setLayout(&layout_);

	const shared_ptr<sigrok::Device> device = session_.device()->device();
	assert(device);

	// Collect a set of signals
	map<shared_ptr<Channel>, shared_ptr<SignalBase> > signal_map;

	unordered_set< shared_ptr<SignalBase> > sigs;
	for (const shared_ptr<data::SignalBase> b : session_.signalbases())
		sigs.insert(b);

	for (const shared_ptr<SignalBase> &sig : sigs)
		signal_map[sig->channel()] = sig;

	// Populate channel groups
	for (auto entry : device->channel_groups()) {
		shared_ptr<ChannelGroup> group = entry.second;
		// Make a set of signals, and removed this signals from the
		// signal map.
		vector< shared_ptr<SignalBase> > group_sigs;
		for (auto channel : group->channels()) {
			const auto iter = signal_map.find(channel);

			if (iter == signal_map.end())
				break;

			group_sigs.push_back((*iter).second);
			signal_map.erase(iter);
		}

		populate_group(group, group_sigs);
	}

	// Make a vector of the remaining channels
	vector< shared_ptr<SignalBase> > global_sigs;
	for (auto channel : device->channels()) {
		const map<shared_ptr<Channel>, shared_ptr<SignalBase> >::
			const_iterator iter = signal_map.find(channel);
		if (iter != signal_map.end())
			global_sigs.push_back((*iter).second);
	}

	// Create a group
	populate_group(nullptr, global_sigs);

	// Create the enable/disable all buttons
	connect(&enable_all_channels_, SIGNAL(clicked()),
		this, SLOT(enable_all_channels()));
	connect(&disable_all_channels_, SIGNAL(clicked()),
		this, SLOT(disable_all_channels()));

	enable_all_channels_.setFlat(true);
	disable_all_channels_.setFlat(true);

	buttons_bar_.addWidget(&enable_all_channels_);
	buttons_bar_.addWidget(&disable_all_channels_);
	buttons_bar_.addStretch(1);

	layout_.addRow(&buttons_bar_);

	// Connect the check-box signal mapper
	connect(&check_box_mapper_, SIGNAL(mapped(QWidget*)),
		this, SLOT(on_channel_checked(QWidget*)));
}

void Channels::set_all_channels(bool set)
{
	updating_channels_ = true;

	for (map<QCheckBox*, shared_ptr<SignalBase> >::const_iterator i =
			check_box_signal_map_.begin();
			i != check_box_signal_map_.end(); i++) {
		const shared_ptr<SignalBase> sig = (*i).second;
		assert(sig);

		sig->set_enabled(set);
		(*i).first->setChecked(set);
	}

	updating_channels_ = false;
}

void Channels::populate_group(shared_ptr<ChannelGroup> group,
	const vector< shared_ptr<SignalBase> > sigs)
{
	using pv::binding::Device;

	// Only bind options if this is a group. We don't do it for general
	// options, because these properties are shown in the device config
	// popup.
	shared_ptr<Device> binding;
	if (group)
		binding = shared_ptr<Device>(new Device(group));

	// Create a title if the group is going to have any content
	if ((!sigs.empty() || (binding && !binding->properties().empty())) &&
		group)
		layout_.addRow(new QLabel(
			QString("<h3>%1</h3>").arg(group->name().c_str())));

	// Create the channel group grid
	QGridLayout *const channel_grid =
		create_channel_group_grid(sigs);
	layout_.addRow(channel_grid);

	// Create the channel group options
	if (binding)
	{
		binding->add_properties_to_form(&layout_, true);
		group_bindings_.push_back(binding);
	}
}

QGridLayout* Channels::create_channel_group_grid(
	const vector< shared_ptr<SignalBase> > sigs)
{
	int row = 0, col = 0;
	QGridLayout *const grid = new QGridLayout();

	for (const shared_ptr<SignalBase>& sig : sigs) {
		assert(sig);

		QCheckBox *const checkbox = new QCheckBox(sig->name());
		check_box_mapper_.setMapping(checkbox, checkbox);
		connect(checkbox, SIGNAL(toggled(bool)),
			&check_box_mapper_, SLOT(map()));

		grid->addWidget(checkbox, row, col);

		check_box_signal_map_[checkbox] = sig;

		if (++col >= 8)
			col = 0, row++;
	}

	return grid;
}

void Channels::showEvent(QShowEvent *event)
{
	pv::widgets::Popup::showEvent(event);

	updating_channels_ = true;

	for (map<QCheckBox*, shared_ptr<SignalBase> >::const_iterator i =
			check_box_signal_map_.begin();
			i != check_box_signal_map_.end(); i++) {
		const shared_ptr<SignalBase> sig = (*i).second;
		assert(sig);

		(*i).first->setChecked(sig->enabled());
	}

	updating_channels_ = false;
}

void Channels::on_channel_checked(QWidget *widget)
{
	if (updating_channels_)
		return;

	QCheckBox *const check_box = (QCheckBox*)widget;
	assert(check_box);

	// Look up the signal of this check-box
	map< QCheckBox*, shared_ptr<SignalBase> >::const_iterator iter =
		check_box_signal_map_.find((QCheckBox*)check_box);
	assert(iter != check_box_signal_map_.end());

	const shared_ptr<SignalBase> s = (*iter).second;
	assert(s);

	s->set_enabled(check_box->isChecked());
}

void Channels::enable_all_channels()
{
	set_all_channels(true);
}

void Channels::disable_all_channels()
{
	set_all_channels(false);
}

} // popups
} // pv
