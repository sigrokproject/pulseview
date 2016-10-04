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

private:
	void closeEvent(QCloseEvent *event);

	virtual QMenu* createPopupMenu();

	virtual bool restoreState(const QByteArray &state, int version = 0);

private Q_SLOTS:
	void on_add_view(const QString &title, views::ViewType type,
		Session *session);

	void on_focus_changed();

	void on_new_session();
	void on_session_name_changed();
	void on_new_view(Session *session);
	void on_view_close_clicked();

	void on_actionViewStickyScrolling_triggered();

	void on_actionViewColouredBg_triggered();

	void on_actionAbout_triggered();

private:
	DeviceManager &device_manager_;

	std::list< std::shared_ptr<Session> > sessions_;

	std::map< QDockWidget*,	std::shared_ptr<views::ViewBase> > view_docks_;

	std::map< std::shared_ptr<Session>, QMainWindow*> session_windows_;

	QTabWidget session_selector_;

	QAction *const action_view_sticky_scrolling_;
	QAction *const action_view_coloured_bg_;
	QAction *const action_about_;
};

} // namespace pv

#endif // PULSEVIEW_PV_MAINWINDOW_HPP
