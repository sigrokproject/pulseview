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

#include "probes.h"

#include <pv/sigsession.h>
#include <pv/view/signal.h>

using namespace boost;
using namespace Qt;
using namespace std;

namespace pv {
namespace popups {

Probes::Probes(SigSession &session, QWidget *parent) :
	Popup(parent),
	_session(session),
	_layout(this),
	_probes(this),
	_probes_bar(this),
	_enable_all_probes(this),
	_disable_all_probes(this)
{
	setLayout(&_layout);

	connect(&_enable_all_probes, SIGNAL(clicked()),
		this, SLOT(enable_all_probes()));
	connect(&_disable_all_probes, SIGNAL(clicked()),
		this, SLOT(disable_all_probes()));

	_layout.addWidget(&_probes);

	_enable_all_probes.setText(tr("Enable All"));
	_probes_bar.addWidget(&_enable_all_probes);

	_disable_all_probes.setText(tr("Disable All"));
	_probes_bar.addWidget(&_disable_all_probes);

	_layout.addWidget(&_probes_bar);

	const vector< shared_ptr<pv::view::Signal> > sigs =
		_session.get_signals();
	for (unsigned int i = 0; i < sigs.size(); i++)
	{
		const shared_ptr<pv::view::Signal> &s = sigs[i];
		assert(s);
		QListWidgetItem *const item = new QListWidgetItem(
			s->get_name(), &_probes);
		assert(item);
		item->setData(UserRole, qVariantFromValue(i));
		item->setCheckState(s->enabled() ? Checked : Unchecked);
		_probes.addItem(item);
	}

	connect(&_probes, SIGNAL(itemChanged(QListWidgetItem*)),
		this, SLOT(item_changed(QListWidgetItem*)));
}

void Probes::set_all_probes(bool set)
{
	using pv::view::Signal;

	const vector< shared_ptr<pv::view::Signal> > sigs =
		_session.get_signals();
	for (unsigned int i = 0; i < sigs.size(); i++)
	{
		const shared_ptr<pv::view::Signal> &s = sigs[i];
		assert(s);
		s->enable(set);

		QListWidgetItem *const item = _probes.item(i);
		assert(item);
		item->setCheckState(set ? Qt::Checked : Qt::Unchecked);
	}
}

void Probes::item_changed(QListWidgetItem *item)
{
	using pv::view::Signal;

	assert(item);
	const vector< shared_ptr<pv::view::Signal> > sigs =
		_session.get_signals();
	const shared_ptr<pv::view::Signal> s = sigs[
		item->data(UserRole).value<unsigned int>()];
	assert(s);
	s->enable(item->checkState() == Checked);
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
