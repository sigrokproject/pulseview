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

#ifndef PULSEVIEW_PV_MAINWINDOW_H
#define PULSEVIEW_PV_MAINWINDOW_H

#include <list>

#include <boost/weak_ptr.hpp>

#include <QMainWindow>
#include <QSignalMapper>

#include "sigsession.h"

class QAction;
class QMenuBar;
class QMenu;
class QVBoxLayout;
class QStatusBar;
class QToolBar;
class QWidget;

namespace pv {

class DeviceManager;

namespace toolbars {
class ContextBar;
class SamplingBar;
}

namespace view {
class View;
}

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(DeviceManager &device_manager,
		const char *open_file_name = NULL,
		QWidget *parent = 0);

private:
	void setup_ui();

	void session_error(const QString text, const QString info_text);

	/**
	 * Updates the device list in the sampling bar, and updates the
	 * selection.
	 * @param selected_device The device to select, or NULL if the
	 * first device in the device list should be selected.
	 */
	void update_device_list(
		struct sr_dev_inst *selected_device = NULL);

	static gint decoder_name_cmp(gconstpointer a, gconstpointer b);
	void setup_add_decoders(QMenu *parent);

private slots:
	void load_file(QString file_name);


	void show_session_error(
		const QString text, const QString info_text);

	void on_actionOpen_triggered();
	void on_actionQuit_triggered();

	void on_actionConnect_triggered();

	void on_actionViewZoomIn_triggered();

	void on_actionViewZoomOut_triggered();

	void on_actionViewShowCursors_triggered();

	void on_actionAbout_triggered();

	void device_selected();

	void add_decoder(QObject *action);

	void run_stop();

	void capture_state_changed(int state);

	void view_selection_changed();

private:
	DeviceManager &_device_manager;

	SigSession _session;

	pv::view::View *_view;

	QMenuBar *_menu_bar;
	QMenu *_menu_file;
	QAction *_action_open;
	QAction *_action_connect;
	QAction *_action_quit;

	QMenu *_menu_view;
	QAction *_action_view_zoom_in;
	QAction *_action_view_zoom_out;
	QAction *_action_view_show_cursors;

	QMenu *_menu_decoders;
	QMenu *_menu_decoders_add;
	QSignalMapper _decoders_add_mapper;

	QMenu *_menu_help;
	QAction *_action_about;

	QWidget *_central_widget;
	QVBoxLayout *_vertical_layout;

	QToolBar *_toolbar;
	toolbars::SamplingBar *_sampling_bar;
	toolbars::ContextBar *_context_bar;
};

} // namespace pv

#endif // PULSEVIEW_PV_MAINWINDOW_H
