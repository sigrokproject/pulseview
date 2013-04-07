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

#ifndef PULSEVIEW_PV_DEVICEOPTIONS_H
#define PULSEVIEW_PV_DEVICEOPTIONS_H

#include <QDialog>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QListWidget>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

#include <pv/prop/binding/deviceoptions.h>

namespace pv {
namespace dialogs {

class DeviceOptions : public QDialog
{
	Q_OBJECT

public:
	DeviceOptions(QWidget *parent, struct sr_dev_inst *sdi);

protected:
	void accept();

private:

	QWidget* get_property_form();

	void setup_probes();

	void set_all_probes(bool set);

private slots:
	void enable_all_probes();
	void disable_all_probes();

private:
	struct sr_dev_inst *const _sdi;

	QVBoxLayout _layout;

	QGroupBox _probes_box;
	QVBoxLayout _probes_box_layout;
	QListWidget _probes;
	QToolBar _probes_bar;
	QToolButton _enable_all_probes;
	QToolButton _disable_all_probes;

	QGroupBox _props_box;
	QVBoxLayout _props_box_layout;

	QDialogButtonBox _button_box;

	pv::prop::binding::DeviceOptions _device_options_binding;
};

} // namespace dialogs
} // namespace pv

#endif // PULSEVIEW_PV_DEVICEOPTIONS_H
