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
#include <QCloseEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QMenu>
#include <QMenuBar>
#include <QSettings>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

#include "mainwindow.hpp"

#include "devicemanager.hpp"
#include "dialogs/about.hpp"
#include "dialogs/connect.hpp"
#include "dialogs/storeprogress.hpp"
#include "toolbars/samplingbar.hpp"
#include "view/logicsignal.hpp"
#include "view/view.hpp"
#ifdef ENABLE_DECODE
#include "widgets/decodermenu.hpp"
#endif

#include <inttypes.h>
#include <stdint.h>
#include <stdarg.h>
#include <glib.h>
#include <libsigrok/libsigrok.hpp>

using std::list;
using std::map;
using std::pair;
using std::shared_ptr;
using std::string;

using sigrok::Device;
using sigrok::Error;
using sigrok::HardwareDevice;

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
	device_manager_(device_manager),
	session_(device_manager)
{
	setup_ui();
	restore_ui_settings();
	if (open_file_name) {
		const QString s(QString::fromUtf8(open_file_name));
		QMetaObject::invokeMethod(this, "load_file",
			Qt::QueuedConnection,
			Q_ARG(QString, s));
	}
}

void MainWindow::run_stop()
{
	switch(session_.get_capture_state()) {
	case Session::Stopped:
		session_.start_capture([&](QString message) {
			session_error("Capture failed", message); });
		break;

	case Session::AwaitingTrigger:
	case Session::Running:
		session_.stop_capture();
		break;
	}
}

void MainWindow::select_device(shared_ptr<Device> device)
{
	try {
		session_.set_device(device);
	} catch(const QString &e) {
		QMessageBox msg(this);
		msg.setText(e);
		msg.setInformativeText(tr("Failed to Select Device"));
		msg.setStandardButtons(QMessageBox::Ok);
		msg.setIcon(QMessageBox::Warning);
		msg.exec();
	}
}

void MainWindow::setup_ui()
{
	setObjectName(QString::fromUtf8("MainWindow"));

	// Set the window icon
	QIcon icon;
	icon.addFile(QString::fromUtf8(":/icons/sigrok-logo-notext.png"),
		QSize(), QIcon::Normal, QIcon::Off);
	setWindowIcon(icon);

	// Setup the central widget
	central_widget_ = new QWidget(this);
	vertical_layout_ = new QVBoxLayout(central_widget_);
	vertical_layout_->setSpacing(6);
	vertical_layout_->setContentsMargins(0, 0, 0, 0);
	setCentralWidget(central_widget_);

	view_ = new pv::view::View(session_, this);

	vertical_layout_->addWidget(view_);

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
	action_view_show_cursors->setChecked(view_->cursors_shown());
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
	toolbar->setObjectName(QString::fromUtf8("MainToolbar"));
	toolbar->addAction(action_open);
	toolbar->addAction(action_save_as);
	toolbar->addSeparator();
	toolbar->addAction(action_view_zoom_in);
	toolbar->addAction(action_view_zoom_out);
	toolbar->addAction(action_view_zoom_fit);
	toolbar->addAction(action_view_zoom_one_to_one);
	addToolBar(toolbar);

	// Setup the sampling bar
	sampling_bar_ = new toolbars::SamplingBar(session_, *this);

	// Populate the device list and select the initially selected device
	update_device_list();

	addToolBar(sampling_bar_);

	// Set the title
	setWindowTitle(tr("PulseView"));

	// Setup session_ events
	connect(&session_, SIGNAL(capture_state_changed(int)), this,
		SLOT(capture_state_changed(int)));
	connect(&session_, SIGNAL(device_selected()), this,
		SLOT(device_selected()));
}

void MainWindow::save_ui_settings()
{
	QSettings settings;

	map<string, string> dev_info;
	list<string> key_list;

	settings.beginGroup("MainWindow");
	settings.setValue("state", saveState());
	settings.setValue("geometry", saveGeometry());
	settings.endGroup();

	if (session_.device()) {
		settings.beginGroup("Device");
		key_list.push_back("vendor");
		key_list.push_back("model");
		key_list.push_back("version");
		key_list.push_back("serial_num");
		key_list.push_back("connection_id");

		dev_info = device_manager_.get_device_info(
			session_.device());

		for (string key : key_list) {

			if (dev_info.count(key))
				settings.setValue(QString::fromUtf8(key.c_str()),
						QString::fromUtf8(dev_info.at(key).c_str()));
			else
				settings.remove(QString::fromUtf8(key.c_str()));
		}

		settings.endGroup();
	}
}

void MainWindow::restore_ui_settings()
{
	QSettings settings;

	shared_ptr<HardwareDevice> device;

	map<string, string> dev_info;
	list<string> key_list;
	string value;

	settings.beginGroup("MainWindow");

	if (settings.contains("geometry")) {
		restoreGeometry(settings.value("geometry").toByteArray());
		restoreState(settings.value("state").toByteArray());
	} else
		resize(1000, 720);

	settings.endGroup();

	// Re-select last used device if possible.
	settings.beginGroup("Device");
	key_list.push_back("vendor");
	key_list.push_back("model");
	key_list.push_back("version");
	key_list.push_back("serial_num");
	key_list.push_back("connection_id");

	for (string key : key_list) {
		if (!settings.contains(QString::fromUtf8(key.c_str())))
			continue;

		value = settings.value(QString::fromUtf8(key.c_str())).toString().toStdString();

		if (value.size() > 0)
			dev_info.insert(std::make_pair(key, value));
	}

	device = device_manager_.find_device_from_info(dev_info);

	if (device) {
		select_device(device);
		update_device_list();
	}

	settings.endGroup();
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
	assert(sampling_bar_);

	shared_ptr<Device> selected_device = session_.device();
	list< shared_ptr<Device> > devices;

	if (device_manager_.devices().size() == 0)
		return;

	std::copy(device_manager_.devices().begin(),
		device_manager_.devices().end(), std::back_inserter(devices));

	if (std::find(devices.begin(), devices.end(), selected_device) ==
		devices.end())
		devices.push_back(selected_device);
	assert(selected_device);

	list< pair< shared_ptr<Device>, string> > device_list;

	for (auto device : devices)
		device_list.push_back(make_pair(
			device, device_manager_.get_display_name(device)));

	sampling_bar_->set_device_list(device_list, selected_device);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
	save_ui_settings();
	event->accept();
}

void MainWindow::load_file(QString file_name)
{
	const QString errorMessage(
		QString("Failed to load file %1").arg(file_name));
	const QString infoMessage;

	try {
		session_.set_file(file_name.toStdString());
	} catch(Error e) {
		show_session_error(tr("Failed to load ") + file_name, e.what());
		session_.set_default_device();
		update_device_list();
		return;
	}

	update_device_list();

	session_.start_capture([&, errorMessage, infoMessage](QString) {
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
	session_.stop_capture();

	QSettings settings;
	const QString dir = settings.value(SettingSaveDirectory).toString();

	// Show the dialog
	const QString file_name = QFileDialog::getSaveFileName(
		this, tr("Save File"), dir, tr("Sigrok Sessions (*.sr)"));

	if (file_name.isEmpty())
		return;

	const QString abs_path = QFileInfo(file_name).absolutePath();
	settings.setValue(SettingSaveDirectory, abs_path);

	StoreProgress *dlg = new StoreProgress(file_name, session_, this);
	dlg->run();
}

void MainWindow::on_actionConnect_triggered()
{
	// Stop any currently running capture session
	session_.stop_capture();

	dialogs::Connect dlg(this, device_manager_);

	// If the user selected a device, select it in the device list. Select the
	// current device otherwise.
	if (dlg.exec())
		select_device(dlg.get_selected_device());

	update_device_list();
}

void MainWindow::on_actionQuit_triggered()
{
	close();
}

void MainWindow::on_actionViewZoomIn_triggered()
{
	view_->zoom(1);
}

void MainWindow::on_actionViewZoomOut_triggered()
{
	view_->zoom(-1);
}

void MainWindow::on_actionViewZoomFit_triggered()
{
	view_->zoom_fit();
}

void MainWindow::on_actionViewZoomOneToOne_triggered()
{
	view_->zoom_one_to_one();
}

void MainWindow::on_actionViewShowCursors_triggered()
{
	assert(view_);

	const bool show = !view_->cursors_shown();
	if(show)
		view_->centre_cursors();

	view_->show_cursors(show);
}

void MainWindow::on_actionAbout_triggered()
{
	dialogs::About dlg(device_manager_.context(), this);
	dlg.exec();
}

void MainWindow::add_decoder(srd_decoder *decoder)
{
#ifdef ENABLE_DECODE
	assert(decoder);
	session_.add_decoder(decoder);
#else
	(void)decoder;
#endif
}

void MainWindow::capture_state_changed(int state)
{
	sampling_bar_->set_capture_state((pv::Session::capture_state)state);
}

void MainWindow::device_selected()
{
	// Set the title to include the device/file name
	const shared_ptr<sigrok::Device> device = session_.device();
	if (!device)
		return;

	const string display_name = device_manager_.get_display_name(device);
	setWindowTitle(tr("%1 - PulseView").arg(display_name.c_str()));
}

} // namespace pv
