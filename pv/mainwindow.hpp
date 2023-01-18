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

#ifndef PULSEVIEW_PV_MAINWINDOW_HPP
#define PULSEVIEW_PV_MAINWINDOW_HPP

#include <list>
#include <map>
#include <memory>

#include <QMainWindow>
#include <QShortcut>
#include <QTabWidget>
#include <QToolButton>

#include "session.hpp"
#include "subwindows/subwindowbase.hpp"

using std::list;
using std::map;
using std::shared_ptr;
using std::string;

struct srd_decoder;

class QVBoxLayout;

namespace pv {

class DeviceManager;

namespace toolbars {
class ContextBar;
class MainBar;
}

namespace view {
class View;
class ViewBase;
}

namespace widgets {
#ifdef ENABLE_DECODE
class DecoderMenu;
#endif
}

using pv::views::ViewBase;
using pv::views::ViewType;

class MainWindow : public QMainWindow
{
	Q_OBJECT

private:
	static const QString WindowTitle;

public:
	explicit MainWindow(DeviceManager &device_manager,
		QWidget *parent = nullptr);

	~MainWindow();

	static void show_session_error(const QString text, const QString info_text);

	shared_ptr<views::ViewBase> get_active_view() const;

	shared_ptr<views::ViewBase> add_view(ViewType type, Session &session);

	void remove_view(shared_ptr<ViewBase> view);

	shared_ptr<subwindows::SubWindowBase> add_subwindow(
		subwindows::SubWindowType type, Session &session);

	shared_ptr<Session> add_session();

	void remove_session(shared_ptr<Session> session);

	void add_session_with_file(string open_file_name, string open_file_format,
		string open_setup_file_name);

	void add_default_session();

	void save_sessions();
	void restore_sessions();

private:
	void setup_ui();
	void update_acq_button(Session *session);

	void save_ui_settings();
	void restore_ui_settings();

	shared_ptr<Session> get_tab_session(int index) const;

	void closeEvent(QCloseEvent *event);

	virtual QMenu* createPopupMenu();

	virtual bool restoreState(const QByteArray &state, int version = 0);

public Q_SLOTS:
	void on_run_stop_clicked();

private Q_SLOTS:
	void on_add_view(ViewType type, Session *session);

	void on_focus_changed();
	void on_focused_session_changed(shared_ptr<Session> session);

	void on_new_session_clicked();
	void on_settings_clicked();

	void on_session_name_changed();
	void on_session_device_changed();
	void on_session_capture_state_changed(int state);

	void on_new_view(Session *session, int view_type);
	void on_view_close_clicked();

	void on_tab_changed(int index);
	void on_tab_close_requested(int index);

	void on_show_decoder_selector(Session *session);
	void on_sub_window_close_clicked();

	void on_view_colored_bg_shortcut();
	void on_view_sticky_scrolling_shortcut();
	void on_view_show_sampling_points_shortcut();
	void on_view_show_analog_minor_grid_shortcut();

	void on_close_current_tab();

private:
	DeviceManager &device_manager_;

	list< shared_ptr<Session> > sessions_;
	shared_ptr<Session> last_focused_session_;

	map< QDockWidget*, shared_ptr<views::ViewBase> > view_docks_;
	map< QDockWidget*, shared_ptr<subwindows::SubWindowBase> > sub_windows_;

	map< shared_ptr<Session>, QMainWindow*> session_windows_;

	QWidget *static_tab_widget_;
	QToolButton *new_session_button_, *run_stop_button_, *settings_button_;
	QTabWidget session_selector_;

	QIcon icon_red_;
	QIcon icon_yellow_;
	QIcon icon_green_;
	QIcon icon_grey_;

	QShortcut *view_sticky_scrolling_shortcut_;
	QShortcut *view_show_sampling_points_shortcut_;
	QShortcut *view_show_analog_minor_grid_shortcut_;
	QShortcut *view_colored_bg_shortcut_;
	QShortcut *run_stop_shortcut_;
	QShortcut *close_application_shortcut_;
	QShortcut *close_current_tab_shortcut_;
};

} // namespace pv

#endif // PULSEVIEW_PV_MAINWINDOW_HPP
