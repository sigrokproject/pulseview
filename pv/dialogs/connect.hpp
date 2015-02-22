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

#ifndef PULSEVIEW_PV_CONNECT_HPP
#define PULSEVIEW_PV_CONNECT_HPP

#include <memory>

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace sigrok {
class Driver;
}

namespace pv {
namespace devices {
class HardwareDevice;
}
}

Q_DECLARE_METATYPE(std::shared_ptr<sigrok::Driver>);
Q_DECLARE_METATYPE(std::shared_ptr<pv::devices::HardwareDevice>);

namespace pv {

class DeviceManager;

namespace dialogs {

class Connect : public QDialog
{
	Q_OBJECT

public:
	Connect(QWidget *parent, pv::DeviceManager &device_manager);

	std::shared_ptr<devices::HardwareDevice> get_selected_device() const;

private:
	void populate_drivers();

	void populate_serials(std::shared_ptr<sigrok::Driver> driver);

	void unset_connection();

	void set_serial_connection(std::shared_ptr<sigrok::Driver> driver);

private Q_SLOTS:
	void device_selected(int index);

	void scan_pressed();

private:
	pv::DeviceManager &device_manager_;

	QVBoxLayout layout_;

	QWidget form_;
	QFormLayout form_layout_;

	QComboBox drivers_;

	QComboBox serial_devices_;

	QPushButton scan_button_;
	QListWidget device_list_;

	QDialogButtonBox button_box_;
};

} // namespace dialogs
} // namespace pv

#endif // PULSEVIEW_PV_CONNECT_HPP
