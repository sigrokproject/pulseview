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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
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

struct srd_decoder;

namespace sigrok {
class Device;
}

namespace pv {

class DeviceManager;

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
	std::shared_ptr<sigrok::Device> selected_device();

	/**
	 * Sets the current list of devices.
	 * @param device the list of devices.
	 * @param selected_device the currently active device.
	 */
	void set_device_list(
		const std::list< std::shared_ptr<sigrok::Device> > &devices,
		std::shared_ptr<sigrok::Device> selected);

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

	std::shared_ptr<sigrok::Device> selected_device_;
	std::vector< std::weak_ptr<sigrok::Device> > devices_;

	QString device_tooltip_;
};

} // widgets
} // pv

#endif // PULSEVIEW_PV_WIDGETS_DEVICETOOLBUTTON_HPP
