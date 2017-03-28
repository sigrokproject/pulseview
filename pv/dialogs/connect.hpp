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

#ifndef PULSEVIEW_PV_CONNECT_HPP
#define PULSEVIEW_PV_CONNECT_HPP

#include <memory>

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

using std::shared_ptr;

namespace sigrok {
class Driver;
}

namespace pv {
namespace devices {
class HardwareDevice;
}
}

Q_DECLARE_METATYPE(shared_ptr<sigrok::Driver>);
Q_DECLARE_METATYPE(shared_ptr<pv::devices::HardwareDevice>);

namespace pv {

class DeviceManager;

namespace dialogs {

class Connect : public QDialog
{
	Q_OBJECT

public:
	Connect(QWidget *parent, pv::DeviceManager &device_manager);

	shared_ptr<devices::HardwareDevice> get_selected_device() const;

private:
	void populate_drivers();

	void populate_serials(shared_ptr<sigrok::Driver> driver);

	void unset_connection();

private Q_SLOTS:
	void driver_selected(int index);

	void serial_toggled(bool checked);
	void tcp_toggled(bool checked);

	void scan_pressed();

private:
	pv::DeviceManager &device_manager_;

	QVBoxLayout layout_;

	QWidget form_;
	QFormLayout form_layout_;

	QComboBox drivers_;

	QComboBox serial_devices_;

	QWidget *tcp_config_;
	QLineEdit *tcp_host_;
	QSpinBox *tcp_port_;
	QCheckBox *tcp_use_vxi_;

	QPushButton scan_button_;
	QListWidget device_list_;

	QDialogButtonBox button_box_;
};

} // namespace dialogs
} // namespace pv

#endif // PULSEVIEW_PV_CONNECT_HPP
