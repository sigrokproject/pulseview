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

#include <cassert>

#ifdef ENABLE_DECODE
#include <libsigrokdecode/libsigrokdecode.h>
#endif

#include <algorithm>
#include <iterator>

#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QFileDialog>
#include <QMessageBox>
#include <QMenu>
#include <QMenuBar>
#include <QSettings>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

#include "mainwindow.h"

#include "devicemanager.h"
#include "device/device.h"
#include "dialogs/about.h"
#include "dialogs/connect.h"
#include "dialogs/storeprogress.h"
#include "toolbars/samplingbar.h"
#include "view/logicsignal.h"
#include "view/view.h"
#ifdef ENABLE_DECODE
#include "widgets/decodermenu.h"
#endif

/* __STDC_FORMAT_MACROS is required for PRIu64 and friends (in C++). */
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <stdint.h>
#include <stdarg.h>
#include <glib.h>
#include <libsigrok/libsigrok.h>

using std::list;
using std::shared_ptr;

namespace pv {

namespace view {
class SelectableItem;
}

const char *MainWindow::SettingOpenDirectory = "MainWindow/OpenDirectory";
const char *MainWindow::SettingSaveDirectory = "MainWindow/SaveDirectory";

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

	_vertical_layout->addWidget(_view);

	// Setup the menu bar
	QMenuBar *const menu_bar = new QMenuBar(this);
	menu_bar->setGeometry(QRect(0, 0, 400, 25));

	// File Menu
	QMenu *const menu_file = new QMenu;
	menu_file->setTitle(tr("&File"));

	QAction *const action_open = new QAction(this);
	action_open->setText(tr("&Open..."));
	action_open->setIcon(QIcon::fromTheme("document-open",
		QIcon(":/icons/document-open.png")));
	action_open->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_O));
	action_open->setObjectName(QString::fromUtf8("actionOpen"));
	menu_file->addAction(action_open);

	QAction *const action_save_as = new QAction(this);
	action_save_as->setText(tr("&Save As..."));
	action_save_as->setIcon(QIcon::fromTheme("document-save-as",
		QIcon(":/icons/document-save-as.png")));
	action_save_as->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_S));
	action_save_as->setObjectName(QString::fromUtf8("actionSaveAs"));
	menu_file->addAction(action_save_as);

	menu_file->addSeparator();

	QAction *const action_connect = new QAction(this);
	action_connect->setText(tr("&Connect to Device..."));
	action_connect->setObjectName(QString::fromUtf8("actionConnect"));
	menu_file->addAction(action_connect);

	menu_file->addSeparator();

	QAction *action_quit = new QAction(this);
	action_quit->setText(tr("&Quit"));
	action_quit->setIcon(QIcon::fromTheme("application-exit",
		QIcon(":/icons/application-exit.png")));
	action_quit->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
	action_quit->setObjectName(QString::fromUtf8("actionQuit"));
	menu_file->addAction(action_quit);

	// View Menu
	QMenu *menu_view = new QMenu;
	menu_view->setTitle(tr("&View"));

	QAction *const action_view_zoom_in = new QAction(this);
	action_view_zoom_in->setText(tr("Zoom &In"));
	action_view_zoom_in->setIcon(QIcon::fromTheme("zoom-in",
		QIcon(":/icons/zoom-in.png")));
	// simply using Qt::Key_Plus shows no + in the menu
	action_view_zoom_in->setShortcut(QKeySequence::ZoomIn);
	action_view_zoom_in->setObjectName(
		QString::fromUtf8("actionViewZoomIn"));
	menu_view->addAction(action_view_zoom_in);

	QAction *const action_view_zoom_out = new QAction(this);
	action_view_zoom_out->setText(tr("Zoom &Out"));
	action_view_zoom_out->setIcon(QIcon::fromTheme("zoom-out",
		QIcon(":/icons/zoom-out.png")));
	action_view_zoom_out->setShortcut(QKeySequence::ZoomOut);
	action_view_zoom_out->setObjectName(
		QString::fromUtf8("actionViewZoomOut"));
	menu_view->addAction(action_view_zoom_out);

	QAction *const action_view_zoom_fit = new QAction(this);
	action_view_zoom_fit->setText(tr("Zoom to &Fit"));
	action_view_zoom_fit->setIcon(QIcon::fromTheme("zoom-fit",
		QIcon(":/icons/zoom-fit.png")));
	action_view_zoom_fit->setShortcut(QKeySequence(Qt::Key_F));
	action_view_zoom_fit->setObjectName(
		QString::fromUtf8("actionViewZoomFit"));
	menu_view->addAction(action_view_zoom_fit);

	QAction *const action_view_zoom_one_to_one = new QAction(this);
	action_view_zoom_one_to_one->setText(tr("Zoom to &One-to-One"));
	action_view_zoom_one_to_one->setIcon(QIcon::fromTheme("zoom-original",
		QIcon(":/icons/zoom-original.png")));
	action_view_zoom_one_to_one->setShortcut(QKeySequence(Qt::Key_O));
	action_view_zoom_one_to_one->setObjectName(
		QString::fromUtf8("actionViewZoomOneToOne"));
	menu_view->addAction(action_view_zoom_one_to_one);

	menu_view->addSeparator();

	QAction *action_view_show_cursors = new QAction(this);
	action_view_show_cursors->setCheckable(true);
	action_view_show_cursors->setChecked(_view->cursors_shown());
	action_view_show_cursors->setShortcut(QKeySequence(Qt::Key_C));
	action_view_show_cursors->setObjectName(
		QString::fromUtf8("actionViewShowCursors"));
	action_view_show_cursors->setText(tr("Show &Cursors"));
	menu_view->addAction(action_view_show_cursors);

	// Decoders Menu
#ifdef ENABLE_DECODE
	QMenu *const menu_decoders = new QMenu;
	menu_decoders->setTitle(tr("&Decoders"));

	pv::widgets::DecoderMenu *const menu_decoders_add =
		new pv::widgets::DecoderMenu(menu_decoders, true);
	menu_decoders_add->setTitle(tr("&Add"));
	connect(menu_decoders_add, SIGNAL(decoder_selected(srd_decoder*)),
		this, SLOT(add_decoder(srd_decoder*)));

	menu_decoders->addMenu(menu_decoders_add);
#endif

	// Help Menu
	QMenu *const menu_help = new QMenu;
	menu_help->setTitle(tr("&Help"));

	QAction *const action_about = new QAction(this);
	action_about->setObjectName(QString::fromUtf8("actionAbout"));
	action_about->setText(tr("&About..."));
	menu_help->addAction(action_about);

	menu_bar->addAction(menu_file->menuAction());
	menu_bar->addAction(menu_view->menuAction());
#ifdef ENABLE_DECODE
	menu_bar->addAction(menu_decoders->menuAction());
#endif
	menu_bar->addAction(menu_help->menuAction());

	setMenuBar(menu_bar);
	QMetaObject::connectSlotsByName(this);

	// Setup the toolbar
	QToolBar *const toolbar = new QToolBar(tr("Main Toolbar"), this);
	toolbar->addAction(action_open);
	toolbar->addAction(action_save_as);
	toolbar->addSeparator();
	toolbar->addAction(action_view_zoom_in);
	toolbar->addAction(action_view_zoom_out);
	toolbar->addAction(action_view_zoom_fit);
	toolbar->addAction(action_view_zoom_one_to_one);
	addToolBar(toolbar);

	// Setup the sampling bar
	_sampling_bar = new toolbars::SamplingBar(_session, this);

	// Populate the device list and select the initially selected device
	update_device_list();

	connect(_sampling_bar, SIGNAL(run_stop()), this,
		SLOT(run_stop()));
	addToolBar(_sampling_bar);

	// Set the title
	setWindowTitle(tr("PulseView"));

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

void MainWindow::update_device_list()
{
	assert(_sampling_bar);

	shared_ptr<pv::device::DevInst> selected_device = _session.get_device();
	list< shared_ptr<device::DevInst> > devices;

	if (_device_manager.devices().size() == 0)
		return;

	std::copy(_device_manager.devices().begin(),
		_device_manager.devices().end(), std::back_inserter(devices));

	if (std::find(devices.begin(), devices.end(), selected_device) ==
		devices.end())
		devices.push_back(selected_device);
	assert(selected_device);

	_sampling_bar->set_device_list(devices, selected_device);
}

void MainWindow::load_file(QString file_name)
{
	const QString errorMessage(
		QString("Failed to load file %1").arg(file_name));
	const QString infoMessage;

	try {
		_session.set_file(file_name.toStdString());
	} catch(QString e) {
		show_session_error(tr("Failed to load ") + file_name, e);
		_session.set_default_device();
		update_device_list();
		return;
	}

	update_device_list();

	_session.start_capture([&, errorMessage, infoMessage](QString) {
		session_error(errorMessage, infoMessage); });
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
	QSettings settings;
	const QString dir = settings.value(SettingOpenDirectory).toString();

	// Show the dialog
	const QString file_name = QFileDialog::getOpenFileName(
		this, tr("Open File"), dir, tr(
			"Sigrok Sessions (*.sr);;"
			"All Files (*.*)"));

	if (!file_name.isEmpty()) {
		load_file(file_name);

		const QString abs_path = QFileInfo(file_name).absolutePath();
		settings.setValue(SettingOpenDirectory, abs_path);
	}
}

void MainWindow::on_actionSaveAs_triggered()
{
	using pv::dialogs::StoreProgress;

	// Stop any currently running capture session
	_session.stop_capture();

	QSettings settings;
	const QString dir = settings.value(SettingSaveDirectory).toString();

	// Show the dialog
	const QString file_name = QFileDialog::getSaveFileName(
		this, tr("Save File"), dir, tr("Sigrok Sessions (*.sr)"));

	if (file_name.isEmpty())
		return;

	const QString abs_path = QFileInfo(file_name).absolutePath();
	settings.setValue(SettingSaveDirectory, abs_path);

	StoreProgress *dlg = new StoreProgress(file_name, _session, this);
	dlg->run();
}

void MainWindow::on_actionConnect_triggered()
{
	// Stop any currently running capture session
	_session.stop_capture();

	dialogs::Connect dlg(this, _device_manager);

	// If the user selected a device, select it in the device list. Select the
	// current device otherwise.
	if (dlg.exec())
		_session.set_device(dlg.get_selected_device());

	update_device_list();
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

void MainWindow::on_actionViewZoomFit_triggered()
{
	_view->zoom_fit();
}

void MainWindow::on_actionViewZoomOneToOne_triggered()
{
	_view->zoom_one_to_one();
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

void MainWindow::add_decoder(srd_decoder *decoder)
{
#ifdef ENABLE_DECODE
	assert(decoder);
	_session.add_decoder(decoder);
#else
	(void)decoder;
#endif
}

void MainWindow::run_stop()
{
	switch(_session.get_capture_state()) {
	case SigSession::Stopped:
		_session.start_capture([&](QString message) {
			session_error("Capture failed", message); });
		break;

	case SigSession::AwaitingTrigger:
	case SigSession::Running:
		_session.stop_capture();
		break;
	}
}

void MainWindow::capture_state_changed(int state)
{
	_sampling_bar->set_capture_state((pv::SigSession::capture_state)state);
}

} // namespace pv
