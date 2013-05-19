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

#ifdef ENABLE_SIGROKDECODE
#include <libsigrokdecode/libsigrokdecode.h>
#endif

#include <boost/bind.hpp>
#include <boost/foreach.hpp>

#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QFileDialog>
#include <QMessageBox>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

#include "mainwindow.h"

#include "devicemanager.h"
#include "dialogs/about.h"
#include "dialogs/connect.h"
#include "toolbars/contextbar.h"
#include "toolbars/samplingbar.h"
#include "view/view.h"

/* __STDC_FORMAT_MACROS is required for PRIu64 and friends (in C++). */
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <stdint.h>
#include <stdarg.h>
#include <glib.h>
#include <libsigrok/libsigrok.h>

using namespace boost;
using namespace std;

namespace pv {

namespace view {
class SelectableItem;
}

MainWindow::MainWindow(DeviceManager &device_manager,
	const char *open_file_name,
	QWidget *parent) :
	QMainWindow(parent),
	_device_manager(device_manager),
	_session(device_manager)
{
	setup_ui();
	if (open_file_name) {
		const QString s(QString::fromUtf8(open_file_name));
		QMetaObject::invokeMethod(this, "load_file",
			Qt::QueuedConnection,
			Q_ARG(QString, s));
	}
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

	// Setup the central widget
	_central_widget = new QWidget(this);
	_vertical_layout = new QVBoxLayout(_central_widget);
	_vertical_layout->setSpacing(6);
	_vertical_layout->setContentsMargins(0, 0, 0, 0);
	setCentralWidget(_central_widget);

	_view = new pv::view::View(_session, this);
	connect(_view, SIGNAL(selection_changed()), this,
		SLOT(view_selection_changed()));

	_vertical_layout->addWidget(_view);

	// Setup the menu bar
	_menu_bar = new QMenuBar(this);
	_menu_bar->setGeometry(QRect(0, 0, 400, 25));

	// File Menu
	_menu_file = new QMenu(_menu_bar);
	_menu_file->setTitle(QApplication::translate(
		"MainWindow", "&File", 0, QApplication::UnicodeUTF8));

	_action_open = new QAction(this);
	_action_open->setText(QApplication::translate(
		"MainWindow", "&Open...", 0, QApplication::UnicodeUTF8));
	_action_open->setIcon(QIcon::fromTheme("document-open",
		QIcon(":/icons/document-open.png")));
	_action_open->setObjectName(QString::fromUtf8("actionOpen"));
	_menu_file->addAction(_action_open);

	_menu_file->addSeparator();

	_action_connect = new QAction(this);
	_action_connect->setText(QApplication::translate(
		"MainWindow", "&Connect to Device...", 0,
		QApplication::UnicodeUTF8));
	_action_connect->setObjectName(QString::fromUtf8("actionConnect"));
	_menu_file->addAction(_action_connect);

	_menu_file->addSeparator();

	_action_quit = new QAction(this);
	_action_quit->setText(QApplication::translate(
		"MainWindow", "&Quit", 0, QApplication::UnicodeUTF8));
	_action_quit->setIcon(QIcon::fromTheme("application-exit",
		QIcon(":/icons/application-exit.png")));
	_action_quit->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
	_action_quit->setObjectName(QString::fromUtf8("actionQuit"));
	_menu_file->addAction(_action_quit);

	// View Menu
	_menu_view = new QMenu(_menu_bar);
	_menu_view->setTitle(QApplication::translate(
		"MainWindow", "&View", 0, QApplication::UnicodeUTF8));

	_action_view_zoom_in = new QAction(this);
	_action_view_zoom_in->setText(QApplication::translate(
		"MainWindow", "Zoom &In", 0, QApplication::UnicodeUTF8));
	_action_view_zoom_in->setIcon(QIcon::fromTheme("zoom-in",
		QIcon(":/icons/zoom-in.png")));
	_action_view_zoom_in->setObjectName(
		QString::fromUtf8("actionViewZoomIn"));
	_menu_view->addAction(_action_view_zoom_in);

	_action_view_zoom_out = new QAction(this);
	_action_view_zoom_out->setText(QApplication::translate(
		"MainWindow", "Zoom &Out", 0, QApplication::UnicodeUTF8));
	_action_view_zoom_out->setIcon(QIcon::fromTheme("zoom-out",
		QIcon(":/icons/zoom-out.png")));
	_action_view_zoom_out->setObjectName(
		QString::fromUtf8("actionViewZoomOut"));
	_menu_view->addAction(_action_view_zoom_out);

	_menu_view->addSeparator();

	_action_view_show_cursors = new QAction(this);
	_action_view_show_cursors->setCheckable(true);
	_action_view_show_cursors->setChecked(_view->cursors_shown());
	_action_view_show_cursors->setShortcut(QKeySequence(Qt::Key_C));
	_action_view_show_cursors->setObjectName(
		QString::fromUtf8("actionViewShowCursors"));
	_action_view_show_cursors->setText(QApplication::translate(
		"MainWindow", "Show &Cursors", 0, QApplication::UnicodeUTF8));
	_menu_view->addAction(_action_view_show_cursors);

	// Help Menu
	_menu_help = new QMenu(_menu_bar);
	_menu_help->setTitle(QApplication::translate(
		"MainWindow", "&Help", 0, QApplication::UnicodeUTF8));

	_action_about = new QAction(this);
	_action_about->setObjectName(QString::fromUtf8("actionAbout"));
	_action_about->setText(QApplication::translate(
		"MainWindow", "&About...", 0, QApplication::UnicodeUTF8));
	_menu_help->addAction(_action_about);

	_menu_bar->addAction(_menu_file->menuAction());
	_menu_bar->addAction(_menu_view->menuAction());
	_menu_bar->addAction(_menu_help->menuAction());

	setMenuBar(_menu_bar);
	QMetaObject::connectSlotsByName(this);

	// Setup the toolbar
	_toolbar = new QToolBar(tr("Main Toolbar"), this);
	_toolbar->addAction(_action_open);
	_toolbar->addSeparator();
	_toolbar->addAction(_action_view_zoom_in);
	_toolbar->addAction(_action_view_zoom_out);
	addToolBar(_toolbar);

	// Setup the sampling bar
	_sampling_bar = new toolbars::SamplingBar(this);

	// Populate the device list and select the initially selected device
	update_device_list();

	connect(_sampling_bar, SIGNAL(device_selected()), this,
		SLOT(device_selected()));
	connect(_sampling_bar, SIGNAL(run_stop()), this,
		SLOT(run_stop()));
	addToolBar(_sampling_bar);

	// Setup the context bar
	_context_bar = new toolbars::ContextBar(this);
	addToolBar(_context_bar);
	insertToolBarBreak(_context_bar);

	// Set the title
	setWindowTitle(QApplication::translate("MainWindow", "PulseView", 0,
		QApplication::UnicodeUTF8));

	// Setup _session events
	connect(&_session, SIGNAL(capture_state_changed(int)), this,
		SLOT(capture_state_changed(int)));

}

void MainWindow::session_error(
	const QString text, const QString info_text)
{
	QMetaObject::invokeMethod(this, "show_session_error",
		Qt::QueuedConnection, Q_ARG(QString, text),
		Q_ARG(QString, info_text));
}

void MainWindow::update_device_list(struct sr_dev_inst *selected_device)
{
	assert(_sampling_bar);

	const list<sr_dev_inst*> &devices = _device_manager.devices();
	_sampling_bar->set_device_list(devices);

	if (!selected_device && !devices.empty()) {
		// Fall back to the first device in the list.
		selected_device = devices.front();

		// Try and find the demo device and select that by default
		BOOST_FOREACH (struct sr_dev_inst *sdi, devices)
			if (strcmp(sdi->driver->name, "demo") == 0) {
				selected_device = sdi;
			}
	}

	if (selected_device) {
		_sampling_bar->set_selected_device(selected_device);
		_session.set_device(selected_device);
	}
}

void MainWindow::load_file(QString file_name)
{
	const QString errorMessage(
		QString("Failed to load file %1").arg(file_name));
	const QString infoMessage;
	_session.load_file(file_name.toStdString(),
		boost::bind(&MainWindow::session_error, this,
			errorMessage, infoMessage));
}

void MainWindow::show_session_error(
	const QString text, const QString info_text)
{
	QMessageBox msg(this);
	msg.setText(text);
	msg.setInformativeText(info_text);
	msg.setStandardButtons(QMessageBox::Ok);
	msg.setIcon(QMessageBox::Warning);
	msg.exec();
}

void MainWindow::on_actionOpen_triggered()
{
	// Enumerate the file formats
	QString filters(tr("Sigrok Sessions (*.sr)"));
	filters.append(tr(";;All Files (*.*)"));

	// Show the dialog
	const QString file_name = QFileDialog::getOpenFileName(
		this, tr("Open File"), "", filters);
	if (!file_name.isEmpty())
		load_file(file_name);
}

void MainWindow::on_actionConnect_triggered()
{
	// Stop any currently running capture session
	_session.stop_capture();

	dialogs::Connect dlg(this, _device_manager);

	// If the user selected a device, select it in the device list. Select the
	// current device otherwise.
	struct sr_dev_inst *const sdi = dlg.exec() ?
		dlg.get_selected_device() : _session.get_device();

	update_device_list(sdi);
}

void MainWindow::on_actionQuit_triggered()
{
	close();
}

void MainWindow::on_actionViewZoomIn_triggered()
{
	_view->zoom(1);
}

void MainWindow::on_actionViewZoomOut_triggered()
{
	_view->zoom(-1);
}

void MainWindow::on_actionViewShowCursors_triggered()
{
	assert(_view);

	const bool show = !_view->cursors_shown();
	if(show)
		_view->centre_cursors();

	_view->show_cursors(show);
}

void MainWindow::on_actionAbout_triggered()
{
	dialogs::About dlg(this);
	dlg.exec();
}

void MainWindow::device_selected()
{
	_session.set_device(_sampling_bar->get_selected_device());
}

void MainWindow::run_stop()
{
	switch(_session.get_capture_state()) {
	case SigSession::Stopped:
		_session.start_capture(_sampling_bar->get_record_length(),
			boost::bind(&MainWindow::session_error, this,
				QString("Capture failed"), _1));
		break;

	case SigSession::Running:
		_session.stop_capture();
		break;
	}
}

void MainWindow::capture_state_changed(int state)
{
	_sampling_bar->set_sampling(state != SigSession::Stopped);
}

void MainWindow::view_selection_changed()
{
	assert(_context_bar);

	const list<weak_ptr<pv::view::SelectableItem> > items(
		_view->selected_items());
	_context_bar->set_selected_items(items);
}

} // namespace pv
