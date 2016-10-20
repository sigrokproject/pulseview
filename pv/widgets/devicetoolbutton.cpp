/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2015 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#include <QTimer>
#include <QToolTip>

#include <libsigrokcxx/libsigrokcxx.hpp>

#include <pv/devicemanager.hpp>
#include <pv/devices/device.hpp>

#include "devicetoolbutton.hpp"

using std::list;
using std::shared_ptr;
using std::string;
using std::weak_ptr;
using std::vector;

using pv::devices::Device;

namespace pv {
namespace widgets {

DeviceToolButton::DeviceToolButton(QWidget *parent,
	DeviceManager &device_manager,
	QAction *connect_action) :
	QToolButton(parent),
	device_manager_(device_manager),
	connect_action_(connect_action),
	menu_(this),
	mapper_(this),
	devices_()
{
	setPopupMode(QToolButton::MenuButtonPopup);
	setMenu(&menu_);
	setDefaultAction(connect_action_);
	setMinimumWidth(QFontMetrics(font()).averageCharWidth() * 24);

	connect(&mapper_, SIGNAL(mapped(QObject*)),
		this, SLOT(on_action(QObject*)));

	connect(&menu_, SIGNAL(hovered(QAction*)),
		this, SLOT(on_menu_hovered(QAction*)));
}

shared_ptr<Device> DeviceToolButton::selected_device()
{
	return selected_device_;
}

void DeviceToolButton::set_device_list(
	const list< shared_ptr<Device> > &devices, shared_ptr<Device> selected)
{
	selected_device_ = selected;
	setText(selected ? QString::fromStdString(
		selected->display_name(device_manager_)) : tr("<No Device>"));
	devices_ = vector< weak_ptr<Device> >(devices.begin(), devices.end());
	update_device_list();
}

void DeviceToolButton::reset()
{
	setText(tr("<No Device>"));
	selected_device_.reset();
	update_device_list();
}

void DeviceToolButton::update_device_list()
{
	menu_.clear();
	menu_.addAction(connect_action_);
	menu_.setDefaultAction(connect_action_);
	menu_.addSeparator();

	for (weak_ptr<Device> dev_weak_ptr : devices_) {
		shared_ptr<Device> dev(dev_weak_ptr.lock());
		if (!dev)
			continue;

		QAction *const a = new QAction(QString::fromStdString(
			dev->display_name(device_manager_)), this);
		a->setCheckable(true);
		a->setChecked(selected_device_ == dev);
		a->setData(qVariantFromValue((void*)dev.get()));
		a->setToolTip(QString::fromStdString(dev->full_name()));
		mapper_.setMapping(a, a);

		connect(a, SIGNAL(triggered()), &mapper_, SLOT(map()));

		menu_.addAction(a);
	}
}

void DeviceToolButton::on_action(QObject *action)
{
	assert(action);

	selected_device_.reset();

	Device *const dev = (Device*)((QAction*)action)->data().value<void*>();
	for (weak_ptr<Device> dev_weak_ptr : devices_) {
		shared_ptr<Device> dev_ptr(dev_weak_ptr);
		if (dev_ptr.get() == dev) {
			selected_device_ = shared_ptr<Device>(dev_ptr);
			break;
		}
	}

	update_device_list();
	setText(QString::fromStdString(
		selected_device_->display_name(device_manager_)));

	device_selected();
}

void DeviceToolButton::on_menu_hovered(QAction *action)
{
	assert(action);

	// Only show the tooltip for device entries (they hold
	// device pointers in their data field)
	if (!action->data().isValid())
		return;

	device_tooltip_ = action->toolTip();

	if (QToolTip::isVisible())
		on_menu_hover_timeout();
	else
		QTimer::singleShot(1000, this, SLOT(on_menu_hover_timeout()));
}

void DeviceToolButton::on_menu_hover_timeout()
{
	if (device_tooltip_.isEmpty())
		return;

	QToolTip::showText(QCursor::pos(), device_tooltip_);
}

} // widgets
} // pv
