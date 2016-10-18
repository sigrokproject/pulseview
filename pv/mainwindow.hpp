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

#ifndef PULSEVIEW_PV_MAINWINDOW_HPP
#define PULSEVIEW_PV_MAINWINDOW_HPP

#include <list>
#include <map>
#include <memory>

#include <QMainWindow>
#include <QSignalMapper>
#include <QToolButton>
#include <QTabWidget>

#include "session.hpp"
#include "views/viewbase.hpp"

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
}

namespace widgets {
#ifdef ENABLE_DECODE
class DecoderMenu;
#endif
}

class MainWindow : public QMainWindow
{
	Q_OBJECT

private:
	static const QString WindowTitle;

public:
	explicit MainWindow(DeviceManager &device_manager,
		std::string open_file_name = std::string(),
		std::string open_file_format = std::string(),
		QWidget *parent = 0);

	~MainWindow();

	QAction* action_view_sticky_scrolling() const;
	QAction* action_view_coloured_bg() const;
	QAction* action_about() const;

	std::shared_ptr<views::ViewBase> get_active_view() const;

	std::shared_ptr<views::ViewBase> add_view(const QString &title,
		views::ViewType type, Session &session);

	std::shared_ptr<Session> add_session();

	void remove_session(std::shared_ptr<Session> session);

private:
	void setup_ui();

	void save_ui_settings();

	void restore_ui_settings();

	std::shared_ptr<Session> get_tab_session(int index) const;

	void closeEvent(QCloseEvent *event);

	virtual QMenu* createPopupMenu();

	virtual bool restoreState(const QByteArray &state, int version = 0);

	void session_error(const QString text, const QString info_text);

private Q_SLOTS:
	void show_session_error(const QString text, const QString info_text);

	void on_add_view(const QString &title, views::ViewType type,
		Session *session);

	void on_focus_changed();
	void on_focused_session_changed(std::shared_ptr<Session> session);

	void on_new_session_clicked();
	void on_run_stop_clicked();

	void on_session_name_changed();
	void on_capture_state_changed(QObject *obj);

	void on_new_view(Session *session);
	void on_view_close_clicked();

	void on_tab_changed(int index);
	void on_tab_close_requested(int index);

	void on_actionViewStickyScrolling_triggered();

	void on_actionViewColouredBg_triggered();

	void on_actionAbout_triggered();

private:
	DeviceManager &device_manager_;

	std::list< std::shared_ptr<Session> > sessions_;
	std::shared_ptr<Session> last_focused_session_;

	std::map< QDockWidget*,	std::shared_ptr<views::ViewBase> > view_docks_;

	std::map< std::shared_ptr<Session>, QMainWindow*> session_windows_;

	QWidget *static_tab_widget_;
	QToolButton *new_session_button_, *run_stop_button_, *settings_button_;
	QTabWidget session_selector_;
	QSignalMapper session_state_mapper_;

	QAction *const action_view_sticky_scrolling_;
	QAction *const action_view_coloured_bg_;
	QAction *const action_about_;

	QIcon icon_red_;
	QIcon icon_green_;
	QIcon icon_grey_;
};

} // namespace pv

#endif // PULSEVIEW_PV_MAINWINDOW_HPP
