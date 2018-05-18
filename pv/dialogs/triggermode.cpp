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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "triggermode.hpp"
#include <pv/session.hpp>

namespace pv {
namespace dialogs {

TriggerMode::TriggerMode(QWidget *parent, Session &session) :
	QDialog(parent),
	session_(session),
	layout_(this),
	form_(this),
	form_layout_(&form_),
	button_box_(QDialogButtonBox::Ok, Qt::Horizontal, this)
{
	setWindowTitle(tr("Trigger Mode"));

	connect(&button_box_, SIGNAL(accepted()), this, SLOT(accept()));

	form_.setLayout(&form_layout_);

// mode

	QCheckBox *chkbox_repetitive_ = new QCheckBox(tr("&Repetitive"), this);

	QVBoxLayout *vbox_mode_ = new QVBoxLayout;
	vbox_mode_->addWidget(chkbox_repetitive_);

	QGroupBox *groupbox_mode_ = new QGroupBox(tr("Select the trigger mode"));
	groupbox_mode_->setLayout(vbox_mode_);
	form_layout_.addRow(groupbox_mode_);

// delay

	rearm_delay_ = new QSpinBox;
	rearm_delay_->setRange(100, 2000000000);
	rearm_delay_->setValue(5000);

	QVBoxLayout *vbox_rearm_delay_ = new QVBoxLayout;
	vbox_rearm_delay_->addWidget(rearm_delay_);

	QGroupBox *groupbox_rearm_delay_ = new QGroupBox(tr("Set the trigger rearm delay in mSec"));
	groupbox_rearm_delay_->setLayout(vbox_rearm_delay_);
	form_layout_.addRow(groupbox_rearm_delay_);

// connections

	connect(chkbox_repetitive_, SIGNAL(toggled(bool)), this, SLOT(chkbox_toggled(bool)));
	connect(rearm_delay_, SIGNAL(valueChanged(int)), this, SLOT(rearm_time_changed(int)));

	layout_.addWidget(&form_);
	layout_.addWidget(&button_box_);

// sync with session

		chkbox_repetitive_->setChecked(session_.get_capture_mode() == pv::Repetitive);
		rearm_delay_->setValue(session_.get_repetitive_rearm_time());
}

void TriggerMode::chkbox_toggled(bool checked)
{
		session_.set_capture_mode(checked ? pv::Repetitive : pv::Single);
}

void TriggerMode::rearm_time_changed(int value)
{
		session_.set_repetitive_rearm_time(value);
}

} // namespace dialogs
} // namespace pv
