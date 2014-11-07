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

#include <cassert>

#include <libsigrok/libsigrok.hpp>

#include "connect.h"

#include "pv/devicemanager.h"

using std::list;
using std::map;
using std::shared_ptr;
using std::string;

using Glib::ustring;
using Glib::Variant;
using Glib::VariantBase;

using sigrok::ConfigKey;
using sigrok::Driver;
using sigrok::Error;
using sigrok::HardwareDevice;

namespace pv {
namespace dialogs {

Connect::Connect(QWidget *parent, pv::DeviceManager &device_manager) :
	QDialog(parent),
	_device_manager(device_manager),
	_layout(this),
	_form(this),
	_form_layout(&_form),
	_drivers(&_form),
	_serial_device(&_form),
	_scan_button(tr("Scan for Devices"), this),
	_device_list(this),
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

	connect(&_scan_button, SIGNAL(pressed()),
		this, SLOT(scan_pressed()));

	setLayout(&_layout);
	_layout.addWidget(&_form);
	_layout.addWidget(&_scan_button);
	_layout.addWidget(&_device_list);
	_layout.addWidget(&_button_box);
}

shared_ptr<HardwareDevice> Connect::get_selected_device() const
{
	const QListWidgetItem *const item = _device_list.currentItem();
	if (!item)
		return shared_ptr<HardwareDevice>();

	return item->data(Qt::UserRole).value<shared_ptr<HardwareDevice>>();
}

void Connect::populate_drivers()
{
	for (auto entry : _device_manager.context()->drivers()) {
		auto name = entry.first;
		auto driver = entry.second;
		/**
		 * We currently only support devices that can deliver
		 * samples at a fixed samplerate i.e. oscilloscopes and
		 * logic analysers.
		 * @todo Add support for non-monotonic devices i.e. DMMs
		 * and sensors.
		 */
		bool supported_device = driver->config_check(
			ConfigKey::SAMPLERATE, ConfigKey::DEVICE_OPTIONS);

		if (supported_device)
			_drivers.addItem(QString("%1 (%2)").arg(
				driver->long_name().c_str()).arg(name.c_str()),
				qVariantFromValue(driver));
	}
}

void Connect::unset_connection()
{
	_device_list.clear();
	_serial_device.hide();
	_form_layout.labelForField(&_serial_device)->hide();
	_button_box.button(QDialogButtonBox::Ok)->setDisabled(true);
}

void Connect::set_serial_connection()
{
	_serial_device.show();
	_form_layout.labelForField(&_serial_device)->show();
}

void Connect::scan_pressed()
{
	_device_list.clear();

	const int index = _drivers.currentIndex();
	if (index == -1)
		return;

	shared_ptr<Driver> driver =
		_drivers.itemData(index).value<shared_ptr<Driver>>();

	assert(driver);

	map<const ConfigKey *, VariantBase> drvopts;

	if (_serial_device.isVisible())
		drvopts[ConfigKey::CONN] = Variant<ustring>::create(
			_serial_device.text().toUtf8().constData());

	list< shared_ptr<HardwareDevice> > devices =
		_device_manager.driver_scan(driver, drvopts);

	for (shared_ptr<HardwareDevice> device : devices)
	{
		assert(device);

		QString text = QString::fromStdString(
			_device_manager.get_display_name(device));
		text += QString(" with %1 channels").arg(device->channels().size());

		QListWidgetItem *const item = new QListWidgetItem(text,
			&_device_list);
		item->setData(Qt::UserRole, qVariantFromValue(device));
		_device_list.addItem(item);
	}

	_device_list.setCurrentRow(0);
	_button_box.button(QDialogButtonBox::Ok)->setDisabled(_device_list.count() == 0);
}

void Connect::device_selected(int index)
{
	shared_ptr<Driver> driver =
		_drivers.itemData(index).value<shared_ptr<Driver>>();

	unset_connection();

	if (driver->config_check(ConfigKey::SERIALCOMM, ConfigKey::SCAN_OPTIONS))
			set_serial_connection();
}

} // namespace dialogs
} // namespace pv
