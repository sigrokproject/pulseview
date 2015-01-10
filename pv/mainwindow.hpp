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
#include <memory>

#include <QMainWindow>

#include "session.hpp"

struct srd_decoder;

class QVBoxLayout;

namespace sigrok {
class Device;
}

namespace pv {

class DeviceManager;

namespace toolbars {
class ContextBar;
class MainBar;
}

namespace view {
class View;
}

namespace widgets {
class DecoderMenu;
}

class MainWindow : public QMainWindow
{
	Q_OBJECT

private:
	/**
	 * Name of the setting used to remember the directory
	 * containing the last file that was opened.
	 */
	static const char *SettingOpenDirectory;

	/**
	 * Name of the setting used to remember the directory
	 * containing the last file that was saved.
	 */
	static const char *SettingSaveDirectory;

public:
	explicit MainWindow(DeviceManager &device_manager,
		const char *open_file_name = NULL,
		QWidget *parent = 0);

	QAction* action_open() const;
	QAction* action_save_as() const;
	QAction* action_connect() const;
	QAction* action_quit() const;
	QAction* action_view_zoom_in() const;
	QAction* action_view_zoom_out() const;
	QAction* action_view_zoom_fit() const;
	QAction* action_view_zoom_one_to_one() const;
	QAction* action_view_show_cursors() const;
	QAction* action_about() const;

	QMenu* menu_decoder_add() const;

	void run_stop();

	void select_device(std::shared_ptr<sigrok::Device> device);

private:
	void setup_ui();

	void save_ui_settings();

	void restore_ui_settings();

	void session_error(const QString text, const QString info_text);

	/**
	 * Updates the device list in the toolbar
	 */
	void update_device_list();

	void closeEvent(QCloseEvent *event);

private Q_SLOTS:
	void load_file(QString file_name);

	void show_session_error(
		const QString text, const QString info_text);

	void on_actionOpen_triggered();
	void on_actionSaveAs_triggered();
	void on_actionQuit_triggered();

	void on_actionConnect_triggered();

	void on_actionViewZoomIn_triggered();

	void on_actionViewZoomOut_triggered();

	void on_actionViewZoomFit_triggered();

	void on_actionViewZoomOneToOne_triggered();

	void on_actionViewShowCursors_triggered();

	void on_actionAbout_triggered();

	void add_decoder(srd_decoder *decoder);

	void capture_state_changed(int state);
	void device_selected();

private:
	DeviceManager &device_manager_;

	Session session_;

	pv::view::View *view_;

	QWidget *central_widget_;
	QVBoxLayout *vertical_layout_;

	toolbars::MainBar *main_bar_;

	QAction *const action_open_;
	QAction *const action_save_as_;
	QAction *const action_connect_;
	QAction *const action_quit_;
	QAction *const action_view_zoom_in_;
	QAction *const action_view_zoom_out_;
	QAction *const action_view_zoom_fit_;
	QAction *const action_view_zoom_one_to_one_;
	QAction *const action_view_show_cursors_;
	QAction *const action_about_;

	QMenu *const menu_decoders_add_;
};

} // namespace pv

#endif // PULSEVIEW_PV_MAINWINDOW_H
