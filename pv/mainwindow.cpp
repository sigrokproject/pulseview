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

extern "C" {
#include <sigrokdecode.h>
}

#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QFileDialog>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

#include "about.h"
#include "mainwindow.h"
#include "samplingbar.h"
#include "pv/view/view.h"

extern "C" {
/* __STDC_FORMAT_MACROS is required for PRIu64 and friends (in C++). */
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <stdint.h>
#include <stdarg.h>
#include <glib.h>
#include <libsigrok/libsigrok.h>
}

namespace pv {

MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent)
{
	setup_ui();
}

void MainWindow::setup_ui()
{
	setObjectName(QString::fromUtf8("MainWindow"));

	resize(1024, 768);

	// Set the window icon
	QIcon icon;
	icon.addFile(QString::fromUtf8(":/icons/sigrok-logo-notext.png"),
		QSize(), QIcon::Normal, QIcon::Off);
	setWindowIcon(icon);

	// Setup the UI actions
	_action_about = new QAction(this);
	_action_about->setObjectName(QString::fromUtf8("actionAbout"));

	_action_view_zoom_in = new QAction(this);
	_action_view_zoom_in->setIcon(QIcon::fromTheme("zoom-in"));
	_action_view_zoom_in->setObjectName(QString::fromUtf8("actionViewZoomIn"));

	_action_view_zoom_out = new QAction(this);
	_action_view_zoom_out->setIcon(QIcon::fromTheme("zoom-out"));
	_action_view_zoom_out->setObjectName(QString::fromUtf8("actionViewZoomOut"));

	_action_open = new QAction(this);
	_action_open->setIcon(QIcon::fromTheme("document-open"));
	_action_open->setObjectName(QString::fromUtf8("actionOpen"));

	// Setup the menu bar
	_menu_bar = new QMenuBar(this);
	_menu_bar->setGeometry(QRect(0, 0, 400, 25));

	_menu_file = new QMenu(_menu_bar);
	_menu_file->addAction(_action_open);

	_menu_view = new QMenu(_menu_bar);
	_menu_view->addAction(_action_view_zoom_in);
	_menu_view->addAction(_action_view_zoom_out);

	_menu_help = new QMenu(_menu_bar);
	_menu_help->addAction(_action_about);

	_menu_bar->addAction(_menu_file->menuAction());
	_menu_bar->addAction(_menu_view->menuAction());
	_menu_bar->addAction(_menu_help->menuAction());

	setMenuBar(_menu_bar);
	QMetaObject::connectSlotsByName(this);

	// Setup the toolbars
	_toolbar = new QToolBar(this);
	_toolbar->addAction(_action_open);
	_toolbar->addSeparator();
	_toolbar->addAction(_action_view_zoom_in);
	_toolbar->addAction(_action_view_zoom_out);
	addToolBar(_toolbar);

	_sampling_bar = new SamplingBar(this);
	connect(_sampling_bar, SIGNAL(run_stop()), this,
		SLOT(run_stop()));
	addToolBar(_sampling_bar);

	// Setup the central widget
	_central_widget = new QWidget(this);
	_vertical_layout = new QVBoxLayout(_central_widget);
	_vertical_layout->setSpacing(6);
	_vertical_layout->setContentsMargins(0, 0, 0, 0);
	setCentralWidget(_central_widget);

	// Setup the status bar
	_status_bar = new QStatusBar(this);
	setStatusBar(_status_bar);

	setWindowTitle(QApplication::translate("MainWindow", "PulseView", 0,
		QApplication::UnicodeUTF8));

	_action_open->setText(QApplication::translate("MainWindow", "&Open...", 0, QApplication::UnicodeUTF8));
	_action_view_zoom_in->setText(QApplication::translate("MainWindow", "Zoom &In", 0, QApplication::UnicodeUTF8));
	_action_view_zoom_out->setText(QApplication::translate("MainWindow", "Zoom &Out", 0, QApplication::UnicodeUTF8));
	_action_about->setText(QApplication::translate("MainWindow", "&About...", 0, QApplication::UnicodeUTF8));

	_menu_file->setTitle(QApplication::translate("MainWindow", "&File", 0, QApplication::UnicodeUTF8));
	_menu_view->setTitle(QApplication::translate("MainWindow", "&View", 0, QApplication::UnicodeUTF8));
	_menu_help->setTitle(QApplication::translate("MainWindow", "&Help", 0, QApplication::UnicodeUTF8));

	_view = new pv::view::View(_session, this);
	_vertical_layout->addWidget(_view);
}

void MainWindow::on_actionOpen_triggered()
{
	QString file_name = QFileDialog::getOpenFileName(
		this, tr("Open File"), "",
		tr("Sigrok Sessions (*.sr)"));
	_session.load_file(file_name.toStdString());
}

void MainWindow::on_actionViewZoomIn_triggered()
{
	_view->zoom(1);
}

void MainWindow::on_actionViewZoomOut_triggered()
{
	_view->zoom(-1);
}

void MainWindow::on_actionAbout_triggered()
{
	About dlg(this);
	dlg.exec();
}

void MainWindow::run_stop()
{
	_session.start_capture(
		_sampling_bar->get_selected_device(),
		_sampling_bar->get_record_length(),
		_sampling_bar->get_sample_rate());
}

} // namespace pv
