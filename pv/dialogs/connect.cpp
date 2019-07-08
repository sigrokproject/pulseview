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

#include <cassert>

#include <libsigrokcxx/libsigrokcxx.hpp>

#include <QGroupBox>
#include <QLabel>
#include <QRadioButton>

#include "connect.hpp"

#include <pv/devicemanager.hpp>
#include <pv/devices/hardwaredevice.hpp>

using std::list;
using std::map;
using std::shared_ptr;
using std::string;

using Glib::ustring;
using Glib::Variant;
using Glib::VariantBase;

using sigrok::ConfigKey;
using sigrok::Driver;

using pv::devices::HardwareDevice;

namespace pv {
namespace dialogs {

Connect::Connect(QWidget *parent, pv::DeviceManager &device_manager) :
	QDialog(parent),
	device_manager_(device_manager),
	layout_(this),
	form_(this),
	form_layout_(&form_),
	drivers_(&form_),
	serial_devices_(&form_),
	scan_button_(tr("&Scan for devices using driver above"), this),
	device_list_(this),
	button_box_(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
		Qt::Horizontal, this)
{
	setWindowTitle(tr("Connect to Device"));

	connect(&button_box_, SIGNAL(accepted()), this, SLOT(accept()));
	connect(&button_box_, SIGNAL(rejected()), this, SLOT(reject()));

	populate_drivers();
	connect(&drivers_, SIGNAL(activated(int)), this, SLOT(driver_selected(int)));

	form_.setLayout(&form_layout_);

	QVBoxLayout *vbox_drv = new QVBoxLayout;
	vbox_drv->addWidget(&drivers_);
	QGroupBox *groupbox_drv = new QGroupBox(tr("Step 1: Choose the driver"));
	groupbox_drv->setLayout(vbox_drv);
	form_layout_.addRow(groupbox_drv);

	QRadioButton *radiobtn_usb = new QRadioButton(tr("&USB"), this);
	QRadioButton *radiobtn_serial = new QRadioButton(tr("Serial &Port"), this);
	QRadioButton *radiobtn_tcp = new QRadioButton(tr("&TCP/IP"), this);

	radiobtn_usb->setChecked(true);

	serial_config_ = new QWidget();
	QHBoxLayout *serial_config_layout = new QHBoxLayout(serial_config_);

	serial_devices_.setEditable(true);
	serial_devices_.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

	serial_baudrate_.setEditable(true);
	serial_baudrate_.addItem("");
	serial_baudrate_.addItem("921600");
	serial_baudrate_.addItem("115200");
	serial_baudrate_.addItem("57600");
	serial_baudrate_.addItem("19200");
	serial_baudrate_.addItem("9600");

	serial_config_layout->addWidget(&serial_devices_);
	serial_config_layout->addWidget(&serial_baudrate_);
	serial_config_layout->addWidget(new QLabel("baud"));
	serial_config_->setEnabled(false);

	tcp_config_ = new QWidget();
	QHBoxLayout *tcp_config_layout = new QHBoxLayout(tcp_config_);
	tcp_host_ = new QLineEdit;
	tcp_host_->setText("192.168.1.100");
	tcp_config_layout->addWidget(tcp_host_);
	tcp_config_layout->addWidget(new QLabel(":"));
	tcp_port_ = new QSpinBox;
	tcp_port_->setRange(1, 65535);
	tcp_port_->setValue(5555);
	tcp_config_layout->addWidget(tcp_port_);

	tcp_config_layout->addSpacing(30);
	tcp_config_layout->addWidget(new QLabel(tr("Protocol:")));
	tcp_protocol_ = new QComboBox();
	tcp_protocol_->addItem("Raw TCP", QVariant("tcp-raw/%1/%2"));
	tcp_protocol_->addItem("VXI", QVariant("vxi/%1/%2"));
	tcp_config_layout->addWidget(tcp_protocol_);
	tcp_config_layout->setContentsMargins(0, 0, 0, 0);
	tcp_config_->setEnabled(false);

	// Let the device list occupy only the minimum space needed
	device_list_.setMaximumHeight(device_list_.minimumSizeHint().height());

	QVBoxLayout *vbox_if = new QVBoxLayout;
	vbox_if->addWidget(radiobtn_usb);
	vbox_if->addWidget(radiobtn_serial);
	vbox_if->addWidget(serial_config_);
	vbox_if->addWidget(radiobtn_tcp);
	vbox_if->addWidget(tcp_config_);

	QGroupBox *groupbox_if = new QGroupBox(tr("Step 2: Choose the interface"));
	groupbox_if->setLayout(vbox_if);
	form_layout_.addRow(groupbox_if);

	QVBoxLayout *vbox_scan = new QVBoxLayout;
	vbox_scan->addWidget(&scan_button_);
	QGroupBox *groupbox_scan = new QGroupBox(tr("Step 3: Scan for devices"));
	groupbox_scan->setLayout(vbox_scan);
	form_layout_.addRow(groupbox_scan);

	QVBoxLayout *vbox_select = new QVBoxLayout;
	vbox_select->addWidget(&device_list_);
	QGroupBox *groupbox_select = new QGroupBox(tr("Step 4: Select the device"));
	groupbox_select->setLayout(vbox_select);
	form_layout_.addRow(groupbox_select);

	unset_connection();

	connect(radiobtn_serial, SIGNAL(toggled(bool)), this, SLOT(serial_toggled(bool)));
	connect(radiobtn_tcp, SIGNAL(toggled(bool)), this, SLOT(tcp_toggled(bool)));
	connect(&scan_button_, SIGNAL(pressed()), this, SLOT(scan_pressed()));

	setLayout(&layout_);

	layout_.addWidget(&form_);
	layout_.addWidget(&button_box_);
}

shared_ptr<HardwareDevice> Connect::get_selected_device() const
{
	const QListWidgetItem *const item = device_list_.currentItem();
	if (!item)
		return shared_ptr<HardwareDevice>();

	return item->data(Qt::UserRole).value<shared_ptr<HardwareDevice>>();
}

void Connect::populate_drivers()
{
	for (auto& entry : device_manager_.context()->drivers()) {
		auto name = entry.first;
		auto driver = entry.second;
		/**
		 * We currently only support devices that can deliver
		 * samples at a fixed samplerate i.e. oscilloscopes and
		 * logic analysers.
		 * @todo Add support for non-monotonic devices i.e. DMMs
		 * and sensors.
		 */
		const auto keys = driver->config_keys();

		bool supported_device = keys.count(ConfigKey::LOGIC_ANALYZER) |
			keys.count(ConfigKey::OSCILLOSCOPE);

		if (supported_device)
			drivers_.addItem(QString("%1 (%2)").arg(
				driver->long_name().c_str(), name.c_str()),
				qVariantFromValue(driver));
	}
}

void Connect::populate_serials(shared_ptr<Driver> driver)
{
	serial_devices_.clear();
	for (auto& serial : device_manager_.context()->serials(driver))
		serial_devices_.addItem(QString("%1 (%2)").arg(
			serial.first.c_str(), serial.second.c_str()),
			QString::fromStdString(serial.first));
}

void Connect::unset_connection()
{
	device_list_.clear();
	button_box_.button(QDialogButtonBox::Ok)->setDisabled(true);
}

void Connect::serial_toggled(bool checked)
{
	serial_devices_.setEnabled(checked);
	serial_baudrate_.setEnabled(checked);
}

void Connect::tcp_toggled(bool checked)
{
	tcp_config_->setEnabled(checked);
}

void Connect::scan_pressed()
{
	device_list_.clear();

	const int index = drivers_.currentIndex();
	if (index == -1)
		return;

	shared_ptr<Driver> driver =
		drivers_.itemData(index).value<shared_ptr<Driver>>();

	assert(driver);

	map<const ConfigKey *, VariantBase> drvopts;

	if (serial_config_->isEnabled()) {
		QString serial;
		const int index = serial_devices_.currentIndex();
		if (index >= 0 && index < serial_devices_.count() &&
		    serial_devices_.currentText() == serial_devices_.itemText(index))
			serial = serial_devices_.itemData(index).toString();
		else
			serial = serial_devices_.currentText();

		drvopts[ConfigKey::CONN] = Variant<ustring>::create(
			serial.toUtf8().constData());

		// Set baud rate if specified
		if (serial_baudrate_.currentText().length() > 0)
			drvopts[ConfigKey::SERIALCOMM] = Variant<ustring>::create(
				QString("%1/8n1").arg(serial_baudrate_.currentText()).toUtf8().constData());
	}

	if (tcp_config_->isEnabled()) {
		QString host = tcp_host_->text();
		QString port = tcp_port_->text();
		if (!host.isEmpty()) {
			QString conn =
				tcp_protocol_->itemData(tcp_protocol_->currentIndex()).toString();

			conn = conn.arg(host, port);

			drvopts[ConfigKey::CONN] = Variant<ustring>::create(
				conn.toUtf8().constData());
		}
	}

	const list< shared_ptr<HardwareDevice> > devices =
		device_manager_.driver_scan(driver, drvopts);

	for (const shared_ptr<HardwareDevice>& device : devices) {
		assert(device);

		QString text = QString::fromStdString(device->display_name(device_manager_));
		text += QString(" with %1 channels").arg(device->device()->channels().size());

		QListWidgetItem *const item = new QListWidgetItem(text, &device_list_);
		item->setData(Qt::UserRole, qVariantFromValue(device));
		device_list_.addItem(item);
	}

	device_list_.setCurrentRow(0);
	button_box_.button(QDialogButtonBox::Ok)->setDisabled(device_list_.count() == 0);
}

void Connect::driver_selected(int index)
{
	shared_ptr<Driver> driver =
		drivers_.itemData(index).value<shared_ptr<Driver>>();

	unset_connection();

	populate_serials(driver);
}

} // namespace dialogs
} // namespace pv
