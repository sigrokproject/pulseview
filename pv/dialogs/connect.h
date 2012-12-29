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

#ifndef PULSEVIEW_PV_CONNECT_H
#define PULSEVIEW_PV_CONNECT_H

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QVBoxLayout>

namespace pv {
namespace dialogs {

class Connect : public QDialog
{
	Q_OBJECT

public:
	Connect(QWidget *parent);

private:
	void populate_drivers();

private slots:
	void device_selected(int index);

	void unset_connection();

	void set_serial_connection();

private:
	QVBoxLayout _layout;

	QWidget _form;
	QFormLayout _form_layout;

	QComboBox _drivers;

	QLineEdit _serial_device;

	QDialogButtonBox _button_box;
};

} // namespace dialogs
} // namespace pv

#endif // PULSEVIEW_PV_CONNECT_H
