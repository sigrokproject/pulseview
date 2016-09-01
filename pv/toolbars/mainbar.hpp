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

#ifndef PULSEVIEW_PV_TOOLBARS_MAINBAR_HPP
#define PULSEVIEW_PV_TOOLBARS_MAINBAR_HPP

#include <stdint.h>

#include <list>
#include <memory>

#include <glibmm/variant.h>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QMenu>
#include <QToolBar>
#include <QToolButton>

#include <pv/session.hpp>
#include <pv/widgets/devicetoolbutton.hpp>
#include <pv/widgets/popuptoolbutton.hpp>
#include <pv/widgets/sweeptimingwidget.hpp>

namespace sigrok {
class Device;
class InputFormat;
class OutputFormat;
}

Q_DECLARE_METATYPE(std::shared_ptr<sigrok::Device>)

class QAction;

namespace pv {

class MainWindow;
class Session;

namespace toolbars {

class MainBar : public QToolBar
{
	Q_OBJECT

private:
	static const uint64_t MinSampleCount;
	static const uint64_t MaxSampleCount;
	static const uint64_t DefaultSampleCount;

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
	MainBar(Session &session, pv::MainWindow &main_window);

	Session &session(void) const;

	void update_device_list();

	void set_capture_state(pv::Session::capture_state state);

	void reset_device_selector();

	void select_device(std::shared_ptr<devices::Device> device);

	void load_init_file(const std::string &file_name,
		const std::string &format);

	QAction* action_new_session() const;
	QAction* action_new_view() const;
	QAction* action_open() const;
	QAction* action_save_as() const;
	QAction* action_save_selection_as() const;
	QAction* action_connect() const;
	QAction* action_quit() const;
	QAction* action_view_zoom_in() const;
	QAction* action_view_zoom_out() const;
	QAction* action_view_zoom_fit() const;
	QAction* action_view_zoom_one_to_one() const;
	QAction* action_view_show_cursors() const;

private:
	void run_stop();

	void select_init_device();

	void load_file(QString file_name,
		std::shared_ptr<sigrok::InputFormat> format = nullptr,
		const std::map<std::string, Glib::VariantBase> &options =
			std::map<std::string, Glib::VariantBase>());

	void save_selection_to_file();

	void update_sample_rate_selector();
	void update_sample_rate_selector_value();
	void update_sample_count_selector();
	void update_device_config_widgets();
	void commit_sample_rate();
	void commit_sample_count();

	void session_error(const QString text, const QString info_text);

	QAction *const action_new_session_;
	QAction *const action_new_view_;
	QAction *const action_open_;
	QAction *const action_save_as_;
	QAction *const action_save_selection_as_;
	QAction *const action_connect_;
	QAction *const action_view_zoom_in_;
	QAction *const action_view_zoom_out_;
	QAction *const action_view_zoom_fit_;
	QAction *const action_view_zoom_one_to_one_;
	QAction *const action_view_show_cursors_;

private Q_SLOTS:
	void show_session_error(const QString text, const QString info_text);

	void capture_state_changed(int state);

	void add_decoder(srd_decoder *decoder);

	void export_file(std::shared_ptr<sigrok::OutputFormat> format,
		bool selection_only = false);
	void import_file(std::shared_ptr<sigrok::InputFormat> format);

	void on_device_selected();
	void on_device_changed();
	void on_sample_count_changed();
	void on_sample_rate_changed();
	void on_run_stop();

	void on_config_changed();

	void on_actionNewSession_triggered();
	void on_actionNewView_triggered();

	void on_actionOpen_triggered();
	void on_actionSaveAs_triggered();
	void on_actionSaveSelectionAs_triggered();

	void on_actionConnect_triggered();

	void on_actionViewZoomIn_triggered();

	void on_actionViewZoomOut_triggered();

	void on_actionViewZoomFit_triggered();

	void on_actionViewZoomOneToOne_triggered();

	void on_actionViewShowCursors_triggered();

	void on_always_zoom_to_fit_changed(bool state);

protected:
	bool eventFilter(QObject *watched, QEvent *event);

Q_SIGNALS:
	void new_session();
	void new_view(Session *session);

private:
	Session &session_;

	pv::widgets::DeviceToolButton device_selector_;

	pv::widgets::PopupToolButton configure_button_;
	QAction *configure_button_action_;

	pv::widgets::PopupToolButton channels_button_;
	QAction *channels_button_action_;

	pv::widgets::SweepTimingWidget sample_count_;
	pv::widgets::SweepTimingWidget sample_rate_;
	bool updating_sample_rate_;
	bool updating_sample_count_;

	bool sample_count_supported_;

	QIcon icon_red_;
	QIcon icon_green_;
	QIcon icon_grey_;
	QToolButton run_stop_button_;
	QAction *run_stop_button_action_;

	QToolButton menu_button_;

#ifdef ENABLE_DECODE
	QMenu *const menu_decoders_add_;
#endif
};

} // namespace toolbars
} // namespace pv

#endif // PULSEVIEW_PV_TOOLBARS_MAINBAR_HPP
