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

#include <boost/foreach.hpp>

#include <QCheckBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>

#include "probes.h"

#include <pv/device/devinst.h>
#include <pv/prop/binding/deviceoptions.h>
#include <pv/sigsession.h>
#include <pv/view/signal.h>

using namespace Qt;

using boost::shared_ptr;
using std::map;
using std::set;
using std::vector;

using pv::view::Signal;

namespace pv {
namespace popups {

Probes::Probes(SigSession &session, QWidget *parent) :
	Popup(parent),
	_session(session),
	_updating_probes(false),
	_enable_all_probes(tr("Enable All"), this),
	_disable_all_probes(tr("Disable All"), this),
	_check_box_mapper(this)
{
	// Create the layout
	setLayout(&_layout);

	shared_ptr<device::DevInst> dev_inst = _session.get_device();
	assert(dev_inst);
	const sr_dev_inst *const sdi = dev_inst->dev_inst();
	assert(sdi);

	// Collect a set of signals
	map<const sr_probe*, shared_ptr<Signal> > signal_map;
	const vector< shared_ptr<Signal> > sigs = _session.get_signals();
	BOOST_FOREACH(const shared_ptr<Signal> &sig, sigs)
		signal_map[sig->probe()] = sig;

	// Populate channel groups
	for (const GSList *g = sdi->channel_groups; g; g = g->next)
	{
		const sr_channel_group *const group =
			(const sr_channel_group*)g->data;
		assert(group);

		// Make a set of signals, and removed this signals from the
		// signal map.
		vector< shared_ptr<Signal> > group_sigs;
		for (const GSList *p = group->probes; p; p = p->next)
		{
			const sr_probe *const probe = (const sr_probe*)p->data;
			assert(probe);

			const map<const sr_probe*, shared_ptr<Signal> >::
				iterator iter = signal_map.find(probe);
			assert(iter != signal_map.end());

			group_sigs.push_back((*iter).second);
			signal_map.erase(iter);
		}

		populate_group(group, group_sigs);
	}

	// Make a vector of the remaining probes
	vector< shared_ptr<Signal> > global_sigs;
	for (const GSList *p = sdi->probes; p; p = p->next)
	{
		const sr_probe *const probe = (const sr_probe*)p->data;
		assert(probe);

		const map<const sr_probe*, shared_ptr<Signal> >::
			const_iterator iter = signal_map.find(probe);
		if (iter != signal_map.end())
			global_sigs.push_back((*iter).second);
	}

	// Create a group
	populate_group(NULL, global_sigs);

	// Create the enable/disable all buttons
	connect(&_enable_all_probes, SIGNAL(clicked()),
		this, SLOT(enable_all_probes()));
	connect(&_disable_all_probes, SIGNAL(clicked()),
		this, SLOT(disable_all_probes()));

	_enable_all_probes.setFlat(true);
	_disable_all_probes.setFlat(true);

	_buttons_bar.addWidget(&_enable_all_probes);
	_buttons_bar.addWidget(&_disable_all_probes);
	_buttons_bar.addStretch(1);

	_layout.addRow(&_buttons_bar);

	// Connect the check-box signal mapper
	connect(&_check_box_mapper, SIGNAL(mapped(QWidget*)),
		this, SLOT(on_probe_checked(QWidget*)));
}

void Probes::set_all_probes(bool set)
{
	_updating_probes = true;

	for (map<QCheckBox*, shared_ptr<Signal> >::const_iterator i =
		_check_box_signal_map.begin();
		i != _check_box_signal_map.end(); i++)
	{
		const shared_ptr<Signal> sig = (*i).second;
		assert(sig);

		sig->enable(set);
		(*i).first->setChecked(set);
	}

	_updating_probes = false;
}

void Probes::populate_group(const sr_channel_group *group,
	const vector< shared_ptr<pv::view::Signal> > sigs)
{
	using pv::prop::binding::DeviceOptions;

	// Only bind options if this is a group. We don't do it for general
	// options, because these properties are shown in the device config
	// popup.
	shared_ptr<DeviceOptions> binding;
	if (group)
		binding = shared_ptr<DeviceOptions>(new DeviceOptions(
			_session.get_device(), group));

	// Create a title if the group is going to have any content
	if ((!sigs.empty() || (binding && !binding->properties().empty())) &&
		group && group->name)
		_layout.addRow(new QLabel(
			QString("<h3>%1</h3>").arg(group->name)));

	// Create the channel group grid
	QGridLayout *const probe_grid =
		create_channel_group_grid(sigs);
	_layout.addRow(probe_grid);

	// Create the channel group options
	if (binding)
	{
		binding->add_properties_to_form(&_layout, true);
		_group_bindings.push_back(binding);
	}
}

QGridLayout* Probes::create_channel_group_grid(
	const vector< shared_ptr<pv::view::Signal> > sigs)
{
	int row = 0, col = 0;
	QGridLayout *const grid = new QGridLayout();

	BOOST_FOREACH(const shared_ptr<pv::view::Signal>& sig, sigs)
	{
		assert(sig);

		QCheckBox *const checkbox = new QCheckBox(sig->get_name());
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

void Probes::showEvent(QShowEvent *e)
{
	pv::widgets::Popup::showEvent(e);

	_updating_probes = true;

	for (map<QCheckBox*, shared_ptr<Signal> >::const_iterator i =
		_check_box_signal_map.begin();
		i != _check_box_signal_map.end(); i++)
	{
		const shared_ptr<Signal> sig = (*i).second;
		assert(sig);

		(*i).first->setChecked(sig->enabled());
	}

	_updating_probes = false;
}

void Probes::on_probe_checked(QWidget *widget)
{
	if (_updating_probes)
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

void Probes::enable_all_probes()
{
	set_all_probes(true);
}

void Probes::disable_all_probes()
{
	set_all_probes(false);
}

} // popups
} // pv
