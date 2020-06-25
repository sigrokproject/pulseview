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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PULSEVIEW_PV_TOOLBARS_MAINBAR_HPP
#define PULSEVIEW_PV_TOOLBARS_MAINBAR_HPP

#include <cstdint>
#include <list>
#include <memory>

#include <glibmm/variant.h>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QMenu>
#include <QToolBar>
#include <QToolButton>

#include <pv/session.hpp>
#include <pv/views/trace/standardbar.hpp>
#include <pv/widgets/devicetoolbutton.hpp>
#include <pv/widgets/popuptoolbutton.hpp>
#include <pv/widgets/sweeptimingwidget.hpp>

using std::shared_ptr;

namespace sigrok {
class Device;
class InputFormat;
class OutputFormat;
}

Q_DECLARE_METATYPE(shared_ptr<sigrok::Device>)

class QAction;

namespace pv {

class MainWindow;
class Session;

namespace views {
namespace trace {
class View;
}
}

namespace toolbars {

class MainBar : public pv::views::trace::StandardBar
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
	MainBar(Session &session, QWidget *parent,
		pv::views::trace::View *view);

	void update_device_list();

	void set_capture_state(pv::Session::capture_state state);

	void reset_device_selector();

	QAction* action_new_view() const;
	QAction* action_open() const;
	QAction* action_save() const;
	QAction* action_save_as() const;
	QAction* action_save_selection_as() const;
	QAction* action_restore_setup() const;
	QAction* action_save_setup() const;
	QAction* action_connect() const;

private:
	void run_stop();

	void select_init_device();

	void save_selection_to_file();

	void update_sample_rate_selector();
	void update_sample_rate_selector_value();
	void update_sample_count_selector();
	void update_device_config_widgets();
	void commit_sample_rate();
	void commit_sample_count();

private Q_SLOTS:
	void show_session_error(const QString text, const QString info_text);

	void export_file(shared_ptr<sigrok::OutputFormat> format,
		bool selection_only = false, QString file_name = "");
	void import_file(shared_ptr<sigrok::InputFormat> format);

	void on_device_selected();
	void on_device_changed();
	void on_capture_state_changed(int state);
	void on_sample_count_changed();
	void on_sample_rate_changed();

	void on_config_changed();

	void on_actionNewView_triggered(QAction* action = nullptr);

	void on_actionOpen_triggered();
	void on_actionSave_triggered();
	void on_actionSaveAs_triggered();
	void on_actionSaveSelectionAs_triggered();

	void on_actionSaveSetup_triggered();
	void on_actionRestoreSetup_triggered();

	void on_actionConnect_triggered();

	void on_add_decoder_clicked();

protected:
	void add_toolbar_widgets();

	bool eventFilter(QObject *watched, QEvent *event);

Q_SIGNALS:
	void new_view(Session *session, int type);
	void show_decoder_selector(Session *session);

private:
	QAction *const action_new_view_;
	QAction *const action_open_;
	QAction *const action_save_;
	QAction *const action_save_as_;
	QAction *const action_save_selection_as_;
	QAction *const action_restore_setup_;
	QAction *const action_save_setup_;
	QAction *const action_connect_;

	QToolButton *new_view_button_, *open_button_, *save_button_;

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

#ifdef ENABLE_DECODE
	QToolButton *add_decoder_button_;
#endif
};

} // namespace toolbars
} // namespace pv

#endif // PULSEVIEW_PV_TOOLBARS_MAINBAR_HPP
