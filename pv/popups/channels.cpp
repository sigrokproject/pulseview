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

#include <QApplication>
#include <QCheckBox>
#include <QFontMetrics>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
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
using std::weak_ptr;

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
	enable_all_channels_(tr("All"), this),
	disable_all_channels_(tr("All"), this),
	enable_all_logic_channels_(tr("Logic"), this),
	disable_all_logic_channels_(tr("Logic"), this),
	enable_all_analog_channels_(tr("Analog"), this),
	disable_all_analog_channels_(tr("Analog"), this),
	enable_all_named_channels_(tr("Named"), this),
	disable_all_unnamed_channels_(tr("Unnamed"), this),
	enable_all_changing_channels_(tr("Changing"), this),
	disable_all_non_changing_channels_(tr("Non-changing"), this),
	check_box_mapper_(this)
{
	// Create the layout
	setLayout(&layout_);

	const shared_ptr<sigrok::Device> device = session_.device()->device();
	assert(device);

	// Collect a set of signals
	map<shared_ptr<Channel>, shared_ptr<SignalBase> > signal_map;

	unordered_set< shared_ptr<SignalBase> > sigs;
	for (const shared_ptr<data::SignalBase>& b : session_.signalbases())
		sigs.insert(b);

	for (const shared_ptr<SignalBase> &sig : sigs)
		signal_map[sig->channel()] = sig;

	// Populate channel groups
	for (auto& entry : device->channel_groups()) {
		const shared_ptr<ChannelGroup> group = entry.second;
		// Make a set of signals and remove these signals from the signal map
		vector< shared_ptr<SignalBase> > group_sigs;
		for (auto& channel : group->channels()) {
			const auto iter = signal_map.find(channel);

			if (iter == signal_map.end())
				break;

			group_sigs.push_back((*iter).second);
			signal_map.erase(iter);
		}

		populate_group(group, group_sigs);
	}

	// Make a vector of the remaining channels
	vector< shared_ptr<SignalBase> > global_analog_sigs, global_logic_sigs;
	for (auto& channel : device->channels()) {
		const map<shared_ptr<Channel>, shared_ptr<SignalBase> >::
			const_iterator iter = signal_map.find(channel);

		if (iter != signal_map.end()) {
			const shared_ptr<SignalBase> signal = (*iter).second;
			if (signal->type() == SignalBase::AnalogChannel)
				global_analog_sigs.push_back(signal);
			else
				global_logic_sigs.push_back(signal);
		}
	}

	// Create the groups for the ungrouped channels
	populate_group(nullptr, global_logic_sigs);
	populate_group(nullptr, global_analog_sigs);

	// Create the enable/disable all buttons
	connect(&enable_all_channels_, SIGNAL(clicked()), this, SLOT(enable_all_channels()));
	connect(&disable_all_channels_, SIGNAL(clicked()), this, SLOT(disable_all_channels()));
	connect(&enable_all_logic_channels_, SIGNAL(clicked()), this, SLOT(enable_all_logic_channels()));
	connect(&disable_all_logic_channels_, SIGNAL(clicked()), this, SLOT(disable_all_logic_channels()));
	connect(&enable_all_analog_channels_, SIGNAL(clicked()), this, SLOT(enable_all_analog_channels()));
	connect(&disable_all_analog_channels_, SIGNAL(clicked()), this, SLOT(disable_all_analog_channels()));
	connect(&enable_all_named_channels_, SIGNAL(clicked()), this, SLOT(enable_all_named_channels()));
	connect(&disable_all_unnamed_channels_, SIGNAL(clicked()), this, SLOT(disable_all_unnamed_channels()));
	connect(&enable_all_changing_channels_, SIGNAL(clicked()),
		this, SLOT(enable_all_changing_channels()));
	connect(&disable_all_non_changing_channels_, SIGNAL(clicked()),
		this, SLOT(disable_all_non_changing_channels()));

	QLabel *label1 = new QLabel(tr("Disable: "));
	filter_buttons_bar_.addWidget(label1, 0, 0);
	filter_buttons_bar_.addWidget(&disable_all_channels_, 0, 1);
	filter_buttons_bar_.addWidget(&disable_all_logic_channels_, 0, 2);
	filter_buttons_bar_.addWidget(&disable_all_analog_channels_, 0, 3);
	filter_buttons_bar_.addWidget(&disable_all_unnamed_channels_, 0, 4);
	filter_buttons_bar_.addWidget(&disable_all_non_changing_channels_, 0, 5);

	QLabel *label2 = new QLabel(tr("Enable: "));
	filter_buttons_bar_.addWidget(label2, 1, 0);
	filter_buttons_bar_.addWidget(&enable_all_channels_, 1, 1);
	filter_buttons_bar_.addWidget(&enable_all_logic_channels_, 1, 2);
	filter_buttons_bar_.addWidget(&enable_all_analog_channels_, 1, 3);
	filter_buttons_bar_.addWidget(&enable_all_named_channels_, 1, 4);
	filter_buttons_bar_.addWidget(&enable_all_changing_channels_, 1, 5);

	layout_.addItem(new QSpacerItem(0, 15, QSizePolicy::Expanding, QSizePolicy::Expanding));
	layout_.addRow(&filter_buttons_bar_);

	// Connect the check-box signal mapper
	connect(&check_box_mapper_, SIGNAL(mapped(QWidget*)),
		this, SLOT(on_channel_checked(QWidget*)));
}

void Channels::set_all_channels(bool set)
{
	updating_channels_ = true;

	for (auto& entry : check_box_signal_map_) {
		QCheckBox *cb = entry.first;
		const shared_ptr<SignalBase> sig = entry.second;
		assert(sig);

		sig->set_enabled(set);
		cb->setChecked(set);
	}

	updating_channels_ = false;
}

void Channels::enable_channels_conditionally(
	function<bool (const shared_ptr<data::SignalBase>)> cond_func)
{
	updating_channels_ = true;

	for (auto& entry : check_box_signal_map_) {
		QCheckBox *cb = entry.first;
		const shared_ptr<SignalBase> sig = entry.second;
		assert(sig);

		if (cond_func(sig)) {
			sig->set_enabled(true);
			cb->setChecked(true);
		}
	}

	updating_channels_ = false;
}

void Channels::disable_channels_conditionally(
	function<bool (const shared_ptr<data::SignalBase>)> cond_func)
{
	updating_channels_ = true;

	for (auto& entry : check_box_signal_map_) {
		QCheckBox *cb = entry.first;
		const shared_ptr<SignalBase> sig = entry.second;
		assert(sig);

		if (cond_func(sig)) {
			sig->set_enabled(false);
			cb->setChecked(false);
		}
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

	QHBoxLayout* group_layout = new QHBoxLayout();
	layout_.addRow(group_layout);

	// Create a title if the group is going to have any content
	if ((!sigs.empty() || (binding && !binding->properties().empty())) && group)
	{
		QLabel *label = new QLabel(
			QString("<h3>%1</h3>").arg(group->name().c_str()));
		group_layout->addWidget(label);
		group_label_map_[group] = label;
	}

	// Create the channel group grid
	int row = 0, col = 0;
	QGridLayout *const grid = new QGridLayout();
	vector<QCheckBox*> group_checkboxes, this_row;

	for (const shared_ptr<SignalBase>& sig : sigs) {
		assert(sig);

		QCheckBox *const checkbox = new QCheckBox(sig->display_name());
		check_box_mapper_.setMapping(checkbox, checkbox);
		connect(checkbox, SIGNAL(toggled(bool)), &check_box_mapper_, SLOT(map()));

		grid->addWidget(checkbox, row, col);

		group_checkboxes.push_back(checkbox);
		this_row.push_back(checkbox);

		check_box_signal_map_[checkbox] = sig;

		weak_ptr<SignalBase> weak_sig(sig);
		connect(checkbox, &QCheckBox::toggled,
				[weak_sig](bool state) {
			auto sig = weak_sig.lock();
			assert(sig);
			sig->set_enabled(state);
			});

		if ((++col >= 8 || &sig == &sigs.back())) {
			// Show buttons if there's more than one row
			if (sigs.size() > 8) {
				QPushButton *row_enable_button = new QPushButton(tr("All"), this);
				grid->addWidget(row_enable_button, row, 8);
				connect(row_enable_button, &QPushButton::clicked,
						[this_row]() {
					for (QCheckBox *box : this_row)
						box->setChecked(true);
					});

				QPushButton *row_disable_button = new QPushButton(tr("None"), this);
				connect(row_disable_button, &QPushButton::clicked,
						[this_row]() {
					for (QCheckBox *box : this_row)
						box->setChecked(false);
					});
				grid->addWidget(row_disable_button, row, 9);
			}

			this_row.clear();
			col = 0, row++;
		}
	}
	layout_.addRow(grid);

	if (sigs.size() > 1) {
		// Create enable all/none buttons
		QPushButton *btn_enable_all, *btn_disable_all;

		btn_enable_all = new QPushButton(tr("All"));
		btn_disable_all = new QPushButton(tr("None"));
		group_layout->addWidget(btn_enable_all);
		group_layout->addWidget(btn_disable_all);

		connect(btn_enable_all, &QPushButton::clicked, [group_checkboxes](){
			for (QCheckBox *box: group_checkboxes)
				box->setChecked(true);
			});

		connect(btn_disable_all, &QPushButton::clicked, [group_checkboxes](){
			for (QCheckBox *box: group_checkboxes)
				box->setChecked(false);
			});
	}

	// Create the channel group options
	if (binding) {
		binding->add_properties_to_form(&layout_, true);
		group_bindings_.push_back(binding);
	}
}

void Channels::showEvent(QShowEvent *event)
{
	pv::widgets::Popup::showEvent(event);

	const shared_ptr<sigrok::Device> device = session_.device()->device();
	assert(device);

	// Update group labels
	for (auto& entry : device->channel_groups()) {
		const shared_ptr<ChannelGroup> group = entry.second;

		try {
			QLabel* label = group_label_map_.at(group);
			label->setText(QString("<h3>%1</h3>").arg(group->name().c_str()));
		} catch (out_of_range&) {
			// Do nothing
		}
	}

	updating_channels_ = true;

	for (auto& entry : check_box_signal_map_) {
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
	enable_channels_conditionally([](const shared_ptr<SignalBase> signal)
		{ return signal->type() == SignalBase::LogicChannel; });
}

void Channels::disable_all_logic_channels()
{
	disable_channels_conditionally([](const shared_ptr<SignalBase> signal)
		{ return signal->type() == SignalBase::LogicChannel; });
}

void Channels::enable_all_analog_channels()
{
	enable_channels_conditionally([](const shared_ptr<SignalBase> signal)
		{ return signal->type() == SignalBase::AnalogChannel; });
}

void Channels::disable_all_analog_channels()
{
	disable_channels_conditionally([](const shared_ptr<SignalBase> signal)
		{ return signal->type() == SignalBase::AnalogChannel; });
}

void Channels::enable_all_named_channels()
{
	enable_channels_conditionally([](const shared_ptr<SignalBase> signal)
		{ return signal->name() != signal->internal_name(); });
}

void Channels::disable_all_unnamed_channels()
{
	disable_channels_conditionally([](const shared_ptr<SignalBase> signal)
		{ return signal->name() == signal->internal_name(); });
}

void Channels::enable_all_changing_channels()
{
	enable_channels_conditionally([](const shared_ptr<SignalBase> signal)
		{
			// Never enable channels without sample data
			if (!signal->has_samples())
				return false;

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

void Channels::disable_all_non_changing_channels()
{
	disable_channels_conditionally([](const shared_ptr<SignalBase> signal)
		{
			// Always disable channels without sample data
			if (!signal->has_samples())
				return true;

			// Non-logic channels are considered to always have a signal
			if (signal->type() != SignalBase::LogicChannel)
				return false;

			const shared_ptr<Logic> logic = signal->logic_data();
			assert(logic);

			// If any of the segments has edges, leave this channel enabled
			for (shared_ptr<LogicSegment> segment : logic->logic_segments()) {
				vector<LogicSegment::EdgePair> edges;

				segment->get_subsampled_edges(edges,
					0, segment->get_sample_count() - 1,
					LogicSegment::MipMapScaleFactor,
					signal->index());

				if (edges.size() > 2)
					return false;
			}

			// No edges detected in any of the segments
			return true;
		});
}

}  // namespace popups
}  // namespace pv
