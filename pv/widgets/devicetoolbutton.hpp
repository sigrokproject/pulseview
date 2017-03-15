/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2014 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#ifndef PULSEVIEW_PV_WIDGETS_DEVICETOOLBUTTON_HPP
#define PULSEVIEW_PV_WIDGETS_DEVICETOOLBUTTON_HPP

#include <list>
#include <memory>
#include <vector>

#include <QAction>
#include <QMenu>
#include <QSignalMapper>
#include <QToolButton>

using std::list;
using std::shared_ptr;
using std::vector;
using std::weak_ptr;

struct srd_decoder;

namespace pv {

class DeviceManager;

namespace devices {
class Device;
}

namespace widgets {

class DeviceToolButton : public QToolButton
{
	Q_OBJECT;

public:
	/**
	 * Constructor
	 * @param parent the parent widget.
	 * @param device_manager the device manager.
	 * @param connect_action the connect-to-device action.
	 */
	DeviceToolButton(QWidget *parent, DeviceManager &device_manager,
		QAction *connect_action);

	/**
	 * Returns a reference to the selected device.
	 */
	shared_ptr<devices::Device> selected_device();

	/**
	 * Sets the current list of devices.
	 * @param device the list of devices.
	 * @param selected_device the currently active device.
	 */
	void set_device_list(
		const list< shared_ptr<devices::Device> > &devices,
		shared_ptr<devices::Device> selected);

	/**
	 * Sets the current device to "no device". Useful for when a selected
	 * device fails to open.
	 */
	void reset();

private:
	/**
	 * Repopulates the menu from the device list.
	 */
	void update_device_list();

private Q_SLOTS:
	void on_action(QObject *action);

	void on_menu_hovered(QAction *action);

	void on_menu_hover_timeout();

Q_SIGNALS:
	void device_selected();

private:
	DeviceManager &device_manager_;
	QAction *const connect_action_;

	QMenu menu_;
	QSignalMapper mapper_;

	shared_ptr<devices::Device> selected_device_;
	vector< weak_ptr<devices::Device> > devices_;

	QString device_tooltip_;
};

} // widgets
} // pv

#endif // PULSEVIEW_PV_WIDGETS_DEVICETOOLBUTTON_HPP
