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

#include <map>

#include <QCheckBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>

#include "channels.h"

#include <pv/prop/binding/deviceoptions.h>
#include <pv/sigsession.h>
#include <pv/view/signal.h>

#include <libsigrok/libsigrok.hpp>

using namespace Qt;

using std::lock_guard;
using std::map;
using std::mutex;
using std::set;
using std::shared_ptr;
using std::vector;

using sigrok::Channel;
using sigrok::ChannelGroup;
using sigrok::Device;

using pv::view::Signal;

namespace pv {
namespace popups {

Channels::Channels(SigSession &session, QWidget *parent) :
	Popup(parent),
	_session(session),
	_updating_channels(false),
	_enable_all_channels(tr("Enable All"), this),
	_disable_all_channels(tr("Disable All"), this),
	_check_box_mapper(this)
{
	// Create the layout
	setLayout(&_layout);

	shared_ptr<sigrok::Device> device = _session.device();
	assert(device);

	// Collect a set of signals
	map<shared_ptr<Channel>, shared_ptr<Signal> > signal_map;

	lock_guard<mutex> lock(_session.signals_mutex());
	const vector< shared_ptr<Signal> > &sigs(_session.signals());

	for (const shared_ptr<Signal> &sig : sigs)
		signal_map[sig->channel()] = sig;

	// Populate channel groups
	for (auto entry : device->channel_groups())
	{
		shared_ptr<ChannelGroup> group = entry.second;
		// Make a set of signals, and removed this signals from the
		// signal map.
		vector< shared_ptr<Signal> > group_sigs;
		for (auto channel : group->channels())
		{
			const auto iter = signal_map.find(channel);

			if (iter == signal_map.end())
				break;

			group_sigs.push_back((*iter).second);
			signal_map.erase(iter);
		}

		populate_group(group, group_sigs);
	}

	// Make a vector of the remaining channels
	vector< shared_ptr<Signal> > global_sigs;
	for (auto channel : device->channels())
	{
		const map<shared_ptr<Channel>, shared_ptr<Signal> >::
			const_iterator iter = signal_map.find(channel);
		if (iter != signal_map.end())
			global_sigs.push_back((*iter).second);
	}

	// Create a group
	populate_group(NULL, global_sigs);

	// Create the enable/disable all buttons
	connect(&_enable_all_channels, SIGNAL(clicked()),
		this, SLOT(enable_all_channels()));
	connect(&_disable_all_channels, SIGNAL(clicked()),
		this, SLOT(disable_all_channels()));

	_enable_all_channels.setFlat(true);
	_disable_all_channels.setFlat(true);

	_buttons_bar.addWidget(&_enable_all_channels);
	_buttons_bar.addWidget(&_disable_all_channels);
	_buttons_bar.addStretch(1);

	_layout.addRow(&_buttons_bar);

	// Connect the check-box signal mapper
	connect(&_check_box_mapper, SIGNAL(mapped(QWidget*)),
		this, SLOT(on_channel_checked(QWidget*)));
}

void Channels::set_all_channels(bool set)
{
	_updating_channels = true;

	for (map<QCheckBox*, shared_ptr<Signal> >::const_iterator i =
		_check_box_signal_map.begin();
		i != _check_box_signal_map.end(); i++)
	{
		const shared_ptr<Signal> sig = (*i).second;
		assert(sig);

		sig->enable(set);
		(*i).first->setChecked(set);
	}

	_updating_channels = false;
}

void Channels::populate_group(shared_ptr<ChannelGroup> group,
	const vector< shared_ptr<pv::view::Signal> > sigs)
{
	using pv::prop::binding::DeviceOptions;

	// Only bind options if this is a group. We don't do it for general
	// options, because these properties are shown in the device config
	// popup.
	shared_ptr<DeviceOptions> binding;
	if (group)
		binding = shared_ptr<DeviceOptions>(new DeviceOptions(group));

	// Create a title if the group is going to have any content
	if ((!sigs.empty() || (binding && !binding->properties().empty())) &&
		group)
		_layout.addRow(new QLabel(
			QString("<h3>%1</h3>").arg(group->name().c_str())));

	// Create the channel group grid
	QGridLayout *const channel_grid =
		create_channel_group_grid(sigs);
	_layout.addRow(channel_grid);

	// Create the channel group options
	if (binding)
	{
		binding->add_properties_to_form(&_layout, true);
		_group_bindings.push_back(binding);
	}
}

QGridLayout* Channels::create_channel_group_grid(
	const vector< shared_ptr<pv::view::Signal> > sigs)
{
	int row = 0, col = 0;
	QGridLayout *const grid = new QGridLayout();

	for (const shared_ptr<pv::view::Signal>& sig : sigs)
	{
		assert(sig);

		QCheckBox *const checkbox = new QCheckBox(sig->name());
		_check_box_mapper.setMapping(checkbox, checkbox);
		connect(checkbox, SIGNAL(toggled(bool)),
			&_check_box_mapper, SLOT(map()));

		grid->addWidget(checkbox, row, col);

		_check_box_signal_map[checkbox] = sig;

		if(++col >= 8)
			col = 0, row++;
	}

	return grid;
}

void Channels::showEvent(QShowEvent *e)
{
	pv::widgets::Popup::showEvent(e);

	_updating_channels = true;

	for (map<QCheckBox*, shared_ptr<Signal> >::const_iterator i =
		_check_box_signal_map.begin();
		i != _check_box_signal_map.end(); i++)
	{
		const shared_ptr<Signal> sig = (*i).second;
		assert(sig);

		(*i).first->setChecked(sig->enabled());
	}

	_updating_channels = false;
}

void Channels::on_channel_checked(QWidget *widget)
{
	if (_updating_channels)
		return;

	QCheckBox *const check_box = (QCheckBox*)widget;
	assert(check_box);

	// Look up the signal of this check-box
	map< QCheckBox*, shared_ptr<Signal> >::const_iterator iter =
		_check_box_signal_map.find((QCheckBox*)check_box);
	assert(iter != _check_box_signal_map.end());

	const shared_ptr<pv::view::Signal> s = (*iter).second;
	assert(s);

	s->enable(check_box->isChecked());
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
