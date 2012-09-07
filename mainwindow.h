/*
 * This file is part of the sigrok project.
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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "sigsession.h"

class SamplingBar;

namespace pv {
namespace view {
class View;
}
}

namespace Ui {
class MainWindow;
}

class QAction;
class QMenuBar;
class QMenu;
class QVBoxLayout;
class QStatusBar;
class QToolBar;
class QWidget;

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget *parent = 0);

private:
	void setup_ui();

private:

	SigSession _session;
	pv::view::View *_view;

	QAction *_action_open;
	QAction *_action_view_zoom_in;
	QAction *_action_view_zoom_out;
	QAction *_action_about;

	QMenuBar *_menu_bar;
	QMenu *_menu_file;
	QMenu *_menu_view;
	QMenu *_menu_help;

	QWidget *_central_widget;
	QVBoxLayout *_vertical_layout;

	QToolBar *_toolbar;
	SamplingBar *_sampling_bar;
	QStatusBar *_status_bar;

private slots:

	void on_actionOpen_triggered();

	void on_actionViewZoomIn_triggered();

	void on_actionViewZoomOut_triggered();

	void on_actionAbout_triggered();

	void run_stop();
};

#endif // MAINWINDOW_H
