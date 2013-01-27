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

#include "hwcap.h"

namespace pv {
namespace dialogs {

HwCap::HwCap(QWidget *parent, struct sr_dev_inst *sdi) :
	QDialog(parent),
	_layout(this),
	_button_box(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
		Qt::Horizontal, this),
	_device_options_binding(sdi)
{
	setWindowTitle(tr("Configure Device"));

	connect(&_button_box, SIGNAL(accepted()), this, SLOT(accept()));
	connect(&_button_box, SIGNAL(rejected()), this, SLOT(reject()));

	setLayout(&_layout);

	QWidget *const form = _device_options_binding.get_form(this);
	_layout.addWidget(form);

	_layout.addWidget(&_button_box);
}

void HwCap::accept()
{
	QDialog::accept();
	_device_options_binding.commit();
}

} // namespace dialogs
} // namespace pv
