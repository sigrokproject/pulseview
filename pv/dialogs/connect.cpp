/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2012-2013 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#include "connect.h"

extern "C" {
/* __STDC_FORMAT_MACROS is required for PRIu64 and friends (in C++). */
#define __STDC_FORMAT_MACROS
#include <glib.h>
#include <libsigrok/libsigrok.h>
}

namespace pv {
namespace dialogs {

Connect::Connect(QWidget *parent) :
	QDialog(parent),
	_layout(this),
	_form(this),
	_form_layout(&_form),
	_drivers(&_form),
	_serial_device(&_form),
	_button_box(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
		Qt::Horizontal, this)
{
	setWindowTitle(tr("Connect to Device"));

	connect(&_button_box, SIGNAL(accepted()), this, SLOT(accept()));
	connect(&_button_box, SIGNAL(rejected()), this, SLOT(reject()));

	populate_drivers();
	connect(&_drivers, SIGNAL(activated(int)),
		this, SLOT(device_selected(int)));

	_form.setLayout(&_form_layout);
	_form_layout.addRow(tr("Driver"), &_drivers);

	_form_layout.addRow(tr("Serial Port"), &_serial_device);

	unset_connection();

	setLayout(&_layout);
	_layout.addWidget(&_form);
	_layout.addWidget(&_button_box);
}

void Connect::populate_drivers()
{
	const int *hwopts;
	struct sr_dev_driver **drivers = sr_driver_list();
	for (int i = 0; drivers[i]; ++i) {
		/**
		 * We currently only support devices that can deliver
		 * samples at a fixed samplerate i.e. oscilloscopes and
		 * logic analysers.
		 * @todo Add support for non-monotonic devices i.e. DMMs
		 * and sensors.
		 */
		bool supported_device = false;
		if ((sr_config_list(drivers[i], SR_CONF_DEVICE_OPTIONS,
			(const void **)&hwopts, NULL) == SR_OK) && hwopts)
			for (int j = 0; hwopts[j]; j++)
				if(hwopts[j] == SR_CONF_SAMPLERATE) {
					supported_device = true;
					break;
				}

		if(supported_device)
			_drivers.addItem(QString("%1 (%2)").arg(
				drivers[i]->longname).arg(drivers[i]->name),
				qVariantFromValue((void*)drivers[i]));
	}
}

void Connect::device_selected(int index)
{
	const int *hwopts;
	const struct sr_hwcap_option *hwo;
	sr_dev_driver *const driver = (sr_dev_driver*)_drivers.itemData(
		index).value<void*>();

	unset_connection();

	if ((sr_config_list(driver, SR_CONF_SCAN_OPTIONS,
		(const void **)&hwopts, NULL) == SR_OK) && hwopts) {

		for (int i = 0; hwopts[i]; i++) {
			switch(hwopts[i]) {
			case SR_CONF_SERIALCOMM:
				set_serial_connection();
				break;

			default:
				continue;
			}

			break;
		}
	}
}

void Connect::unset_connection()
{
	_serial_device.hide();
	_form_layout.labelForField(&_serial_device)->hide();
}

void Connect::set_serial_connection()
{
	_serial_device.show();
	_form_layout.labelForField(&_serial_device)->show();
}


} // namespace dialogs
} // namespace pv
