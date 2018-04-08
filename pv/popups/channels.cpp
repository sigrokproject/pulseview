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

#include <QCheckBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>

#include "channels.hpp"

#include <pv/session.hpp>
#include <pv/binding/device.hpp>
#include <pv/data/logic.hpp>
#include <pv/data/logicsegment.hpp>
#include <pv/data/signalbase.hpp>
#include <pv/devices/device.hpp>

using std::make_shared;
using std::map;
using std::out_of_range;
using std::shared_ptr;
using std::unordered_set;
using std::vector;

using pv::data::SignalBase;
using pv::data::Logic;
using pv::data::LogicSegment;

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
	enable_all_logic_channels_(tr("Enable only logic"), this),
	enable_all_analog_channels_(tr("Enable only analog"), this),
	enable_all_named_channels_(tr("Enable only named"), this),
	enable_all_changing_channels_(tr("Enable only changing"), this),
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
		const shared_ptr<ChannelGroup> group = entry.second;
		// Make a set of signals and remove these signals from the signal map
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
	connect(&enable_all_channels_, SIGNAL(clicked()), this, SLOT(enable_all_channels()));
	connect(&disable_all_channels_, SIGNAL(clicked()), this, SLOT(disable_all_channels()));
	connect(&enable_all_logic_channels_, SIGNAL(clicked()), this, SLOT(enable_all_logic_channels()));
	connect(&enable_all_analog_channels_, SIGNAL(clicked()), this, SLOT(enable_all_analog_channels()));
	connect(&enable_all_named_channels_, SIGNAL(clicked()), this, SLOT(enable_all_named_channels()));
	connect(&enable_all_changing_channels_, SIGNAL(clicked()),
		this, SLOT(enable_all_changing_channels()));

	enable_all_channels_.setFlat(true);
	disable_all_channels_.setFlat(true);
	enable_all_logic_channels_.setFlat(true);
	enable_all_analog_channels_.setFlat(true);
	enable_all_named_channels_.setFlat(true);
	enable_all_changing_channels_.setFlat(true);

	buttons_bar_.addWidget(&enable_all_channels_, 0, 0);
	buttons_bar_.addWidget(&disable_all_channels_, 0, 1);
	buttons_bar_.addWidget(&enable_all_logic_channels_, 1, 0);
	buttons_bar_.addWidget(&enable_all_analog_channels_, 1, 1);
	buttons_bar_.addWidget(&enable_all_named_channels_, 1, 2);
	buttons_bar_.addWidget(&enable_all_changing_channels_, 1, 3);

	layout_.addRow(&buttons_bar_);

	// Connect the check-box signal mapper
	connect(&check_box_mapper_, SIGNAL(mapped(QWidget*)),
		this, SLOT(on_channel_checked(QWidget*)));
}

void Channels::set_all_channels(bool set)
{
	updating_channels_ = true;

	for (auto entry : check_box_signal_map_) {
		QCheckBox *cb = entry.first;
		const shared_ptr<SignalBase> sig = entry.second;
		assert(sig);

		sig->set_enabled(set);
		cb->setChecked(set);
	}

	updating_channels_ = false;
}

void Channels::set_all_channels_conditionally(
	function<bool (const shared_ptr<data::SignalBase>)> cond_func)
{
	updating_channels_ = true;

	for (auto entry : check_box_signal_map_) {
		QCheckBox *cb = entry.first;
		const shared_ptr<SignalBase> sig = entry.second;
		assert(sig);

		const bool state = cond_func(sig);
		sig->set_enabled(state);
		cb->setChecked(state);
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
		binding = make_shared<Device>(group);

	// Create a title if the group is going to have any content
	if ((!sigs.empty() || (binding && !binding->properties().empty())) && group)
	{
		QLabel *label = new QLabel(
			QString("<h3>%1</h3>").arg(group->name().c_str()));
		layout_.addRow(label);
		group_label_map_[group] = label;
	}

	// Create the channel group grid
	QGridLayout *const channel_grid = create_channel_group_grid(sigs);
	layout_.addRow(channel_grid);

	// Create the channel group options
	if (binding) {
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

		QCheckBox *const checkbox = new QCheckBox(sig->display_name());
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

	const shared_ptr<sigrok::Device> device = session_.device()->device();
	assert(device);

	// Update group labels
	for (auto entry : device->channel_groups()) {
		const shared_ptr<ChannelGroup> group = entry.second;

		try {
			QLabel* label = group_label_map_.at(group);
			label->setText(QString("<h3>%1</h3>").arg(group->name().c_str()));
		} catch (out_of_range&) {
			// Do nothing
		}
	}

	updating_channels_ = true;

	for (auto entry : check_box_signal_map_) {
		QCheckBox *cb = entry.first;
		const shared_ptr<SignalBase> sig = entry.second;
		assert(sig);

		// Update the check box
		cb->setChecked(sig->enabled());
		cb->setText(sig->display_name());
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

void Channels::enable_all_logic_channels()
{
	set_all_channels_conditionally([](const shared_ptr<SignalBase> signal)
		{ return signal->type() == SignalBase::LogicChannel; });
}

void Channels::enable_all_analog_channels()
{
	set_all_channels_conditionally([](const shared_ptr<SignalBase> signal)
		{ return signal->type() == SignalBase::AnalogChannel; });
}

void Channels::enable_all_named_channels()
{
	set_all_channels_conditionally([](const shared_ptr<SignalBase> signal)
		{ return signal->name() != signal->internal_name(); });
}

void Channels::enable_all_changing_channels()
{
	set_all_channels_conditionally([](const shared_ptr<SignalBase> signal)
		{
			// Non-logic channels are considered to always have a signal
			if (signal->type() != SignalBase::LogicChannel)
				return true;

			const shared_ptr<Logic> logic = signal->logic_data();
			assert(logic);

			// If any of the segments has edges, enable this channel
			for (shared_ptr<LogicSegment> segment : logic->logic_segments()) {
				vector<LogicSegment::EdgePair> edges;

				segment->get_subsampled_edges(edges,
					0, segment->get_sample_count() - 1,
					LogicSegment::MipMapScaleFactor,
					signal->index());

				if (edges.size() > 2)
					return true;
			}

			// No edges detected in any of the segments
			return false;
		});
}

}  // namespace popups
}  // namespace pv
