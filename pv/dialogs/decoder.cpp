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

#include "decoder.h"

extern "C" {
/* __STDC_FORMAT_MACROS is required for PRIu64 and friends (in C++). */
#define __STDC_FORMAT_MACROS
#include <glib.h>
#include <libsigrok/libsigrok.h>
}

namespace pv {
namespace dialogs {

Decoder::Decoder(QWidget *parent) :
	QDialog(parent),
	_layout(this),
	_form(this),
	_form_layout(&_form),
	_button_box(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
		Qt::Horizontal, this)
{
	setWindowTitle(tr("Configure Decoder"));

	connect(&_button_box, SIGNAL(accepted()), this, SLOT(accept()));
	connect(&_button_box, SIGNAL(rejected()), this, SLOT(reject()));

	_form.setLayout(&_form_layout);

	setLayout(&_layout);
	_layout.addWidget(&_form);
	_layout.addWidget(&_button_box);
}

} // namespace dialogs
} // namespace pv
