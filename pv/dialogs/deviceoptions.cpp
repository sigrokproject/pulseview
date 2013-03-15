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

#include "deviceoptions.h"

#include <QListWidget>

namespace pv {
namespace dialogs {

DeviceOptions::DeviceOptions(QWidget *parent, struct sr_dev_inst *sdi) :
	QDialog(parent),
	_sdi(sdi),
	_layout(this),
	_probes_box(tr("Probes"), this),
	_probes(this),
	_probes_bar(this),
	_enable_all_probes(this),
	_disable_all_probes(this),
	_props_box(tr("Configuration"), this),
	_button_box(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
		Qt::Horizontal, this),
	_device_options_binding(sdi)
{
	setWindowTitle(tr("Configure Device"));

	connect(&_enable_all_probes, SIGNAL(clicked()),
		this, SLOT(enable_all_probes()));
	connect(&_disable_all_probes, SIGNAL(clicked()),
		this, SLOT(disable_all_probes()));

	connect(&_button_box, SIGNAL(accepted()), this, SLOT(accept()));
	connect(&_button_box, SIGNAL(rejected()), this, SLOT(reject()));

	setLayout(&_layout);

	setup_probes();
	_probes_box.setLayout(&_probes_box_layout);
	_probes_box_layout.addWidget(&_probes);

	_enable_all_probes.setText(tr("Enable All"));
	_probes_bar.addWidget(&_enable_all_probes);

	_disable_all_probes.setText(tr("Disable All"));
	_probes_bar.addWidget(&_disable_all_probes);

	_probes_box_layout.addWidget(&_probes_bar);
	_layout.addWidget(&_probes_box);


	_props_box.setLayout(&_props_box_layout);
	_props_box_layout.addWidget(_device_options_binding.get_form(this));
	_layout.addWidget(&_props_box);

	_layout.addWidget(&_button_box);
}

void DeviceOptions::accept()
{
	using namespace Qt;

	QDialog::accept();

	// Commit the probes
	for (int i = 0; i < _probes.count(); i++) {
		const QListWidgetItem *const item = _probes.item(i);
		assert(item);
		sr_probe *const probe = (sr_probe*)
			item->data(UserRole).value<void*>();
		assert(probe);
		probe->enabled = item->checkState() == Checked;
	}

	// Commit the properties
	_device_options_binding.commit();
}

void DeviceOptions::setup_probes()
{
	using namespace Qt;

	for (const GSList *l = _sdi->probes; l; l = l->next) {
		sr_probe *const probe = (sr_probe*)l->data;
		assert(probe);
		QListWidgetItem *const item = new QListWidgetItem(
			probe->name, &_probes);
		assert(item);
		item->setCheckState(probe->enabled ?
			Checked : Unchecked);
		item->setData(UserRole,
			qVariantFromValue((void*)probe));
		_probes.addItem(item);
	}
}

void DeviceOptions::set_all_probes(bool set)
{
	for (int i = 0; i < _probes.count(); i++) {
		QListWidgetItem *const item = _probes.item(i);
		assert(item);
		item->setCheckState(set ? Qt::Checked : Qt::Unchecked);
	}
}

void DeviceOptions::enable_all_probes()
{
	set_all_probes(true);
}

void DeviceOptions::disable_all_probes()
{
	set_all_probes(false);
}

} // namespace dialogs
} // namespace pv
