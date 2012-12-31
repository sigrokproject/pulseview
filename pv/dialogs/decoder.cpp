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

extern "C" {
#include <libsigrokdecode/libsigrokdecode.h>
}

#include "decoder.h"

namespace pv {
namespace dialogs {

Decoder::Decoder(QWidget *parent, const srd_decoder *decoder) :
	QDialog(parent),
	_decoder(decoder),
	_layout(this),
	_form(this),
	_form_layout(&_form),
	_heading(this),
	_button_box(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
		Qt::Horizontal, this)
{
	setWindowTitle(tr("Configure %1").arg(decoder->name));

	_heading.setText(tr("<h3>%1</h3>%2")
		.arg(decoder->longname)
		.arg(decoder->desc));

	connect(&_button_box, SIGNAL(accepted()), this, SLOT(accept()));
	connect(&_button_box, SIGNAL(rejected()), this, SLOT(reject()));

	_form.setLayout(&_form_layout);

	setLayout(&_layout);
	_layout.addWidget(&_heading);
	_layout.addWidget(&_form);
	_layout.addWidget(&_button_box);
}

} // namespace dialogs
} // namespace pv
