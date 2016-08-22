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

#include <boost/algorithm/string/join.hpp>

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

#include <QDockWidget>

#include "mainwindow.hpp"

#include "devicemanager.hpp"
#include "util.hpp"
#include "data/segment.hpp"
#include "devices/hardwaredevice.hpp"
#include "devices/inputfile.hpp"
#include "devices/sessionfile.hpp"
#include "dialogs/about.hpp"
#include "dialogs/connect.hpp"
#include "dialogs/inputoutputoptions.hpp"
#include "dialogs/storeprogress.hpp"
#include "toolbars/mainbar.hpp"
#include "view/logicsignal.hpp"
#include "view/view.hpp"
#include "widgets/exportmenu.hpp"
#include "widgets/importmenu.hpp"
#ifdef ENABLE_DECODE
#include "widgets/decodermenu.hpp"
#endif

#include <inttypes.h>
#include <stdint.h>
#include <stdarg.h>
#include <glib.h>
#include <libsigrokcxx/libsigrokcxx.hpp>

using std::cerr;
using std::endl;
using std::list;
using std::make_shared;
using std::map;
using std::max;
using std::pair;
using std::shared_ptr;
using std::string;
using std::vector;

using boost::algorithm::join;

using sigrok::Error;
using sigrok::OutputFormat;
using sigrok::InputFormat;

namespace pv {

namespace view {
class ViewItem;
}

const char *MainWindow::SettingOpenDirectory = "MainWindow/OpenDirectory";
const char *MainWindow::SettingSaveDirectory = "MainWindow/SaveDirectory";

MainWindow::MainWindow(DeviceManager &device_manager,
	string open_file_name, string open_file_format,
	QWidget *parent) :
	QMainWindow(parent),
	device_manager_(device_manager),
	session_(device_manager),
	action_open_(new QAction(this)),
	action_save_as_(new QAction(this)),
	action_save_selection_as_(new QAction(this)),
	action_connect_(new QAction(this)),
	action_quit_(new QAction(this)),
	action_view_zoom_in_(new QAction(this)),
	action_view_zoom_out_(new QAction(this)),
	action_view_zoom_fit_(new QAction(this)),
	action_view_zoom_one_to_one_(new QAction(this)),
	action_view_sticky_scrolling_(new QAction(this)),
	action_view_coloured_bg_(new QAction(this)),
	action_view_show_cursors_(new QAction(this)),
	action_about_(new QAction(this))
#ifdef ENABLE_DECODE
	, menu_decoders_add_(new pv::widgets::DecoderMenu(this, true))
#endif
{
	qRegisterMetaType<util::Timestamp>("util::Timestamp");

	setup_ui();
	restore_ui_settings();
	if (open_file_name.empty())
		select_init_device();
	else
		load_init_file(open_file_name, open_file_format);
}

MainWindow::~MainWindow()
{
	for (auto entry : view_docks_) {
		const std::shared_ptr<QDockWidget> dock = entry.first;
		dock->setWidget(0);
		const std::shared_ptr<pv::view::View> view = entry.second;
		session_.deregister_view(view);
	}
}

QAction* MainWindow::action_open() const
{
	return action_open_;
}

QAction* MainWindow::action_save_as() const
{
	return action_save_as_;
}

QAction* MainWindow::action_save_selection_as() const
{
	return action_save_selection_as_;
}

QAction* MainWindow::action_connect() const
{
	return action_connect_;
}

QAction* MainWindow::action_quit() const
{
	return action_quit_;
}

QAction* MainWindow::action_view_zoom_in() const
{
	return action_view_zoom_in_;
}

QAction* MainWindow::action_view_zoom_out() const
{
	return action_view_zoom_out_;
}

QAction* MainWindow::action_view_zoom_fit() const
{
	return action_view_zoom_fit_;
}

QAction* MainWindow::action_view_zoom_one_to_one() const
{
	return action_view_zoom_one_to_one_;
}

QAction* MainWindow::action_view_sticky_scrolling() const
{
	return action_view_sticky_scrolling_;
}

QAction* MainWindow::action_view_coloured_bg() const
{
	return action_view_coloured_bg_;
}

QAction* MainWindow::action_view_show_cursors() const
{
	return action_view_show_cursors_;
}

QAction* MainWindow::action_about() const
{
	return action_about_;
}

#ifdef ENABLE_DECODE
QMenu* MainWindow::menu_decoder_add() const
{
	return menu_decoders_add_;
}
#endif

shared_ptr<pv::view::View> MainWindow::get_active_view() const
{
	// If there's only one view, use it...
	if (view_docks_.size() == 1)
		return view_docks_.begin()->second;

	// ...otherwise find the dock widget the widget with focus is contained in
	QObject *w = QApplication::focusWidget();
	QDockWidget *dock = 0;

	while (w) {
	    dock = qobject_cast<QDockWidget*>(w);
	    if (dock)
	        break;
	    w = w->parent();
	}

	// Get the view contained in the dock widget
	for (auto entry : view_docks_)
		if (entry.first.get() == dock)
			return entry.second;

	return shared_ptr<pv::view::View>();
}

shared_ptr<pv::view::View> MainWindow::add_view(const QString &title,
	view::ViewType type, Session &session)
{
	shared_ptr<pv::view::View> v;

	if (type == pv::view::TraceView)
		v = make_shared<pv::view::View>(session, this);

	if (v) {
		shared_ptr<QDockWidget> dock = make_shared<QDockWidget>(title, this);
		dock->setWidget(v.get());
		dock->setObjectName(title);
		addDockWidget(Qt::TopDockWidgetArea, dock.get());
		view_docks_[dock] = v;

		dock->setFeatures(QDockWidget::DockWidgetMovable |
			QDockWidget::DockWidgetFloatable);

		if (type == view::TraceView) {
			connect(&session, SIGNAL(trigger_event(util::Timestamp)), v.get(),
				SLOT(trigger_event(util::Timestamp)));
			connect(v.get(), SIGNAL(sticky_scrolling_changed(bool)), this,
				SLOT(sticky_scrolling_changed(bool)));
			connect(v.get(), SIGNAL(always_zoom_to_fit_changed(bool)), this,
				SLOT(always_zoom_to_fit_changed(bool)));

			v->enable_sticky_scrolling(action_view_sticky_scrolling_->isChecked());
			v->enable_coloured_bg(action_view_coloured_bg_->isChecked());
			action_view_show_cursors_->setChecked(v->cursors_shown());
		}

		session.register_view(v);
	}

	return v;
}

void MainWindow::run_stop()
{
	switch (session_.get_capture_state()) {
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

void MainWindow::select_device(shared_ptr<devices::Device> device)
{
	try {
		if (device)
			session_.set_device(device);
		else
			session_.set_default_device();
	} catch (const QString &e) {
		QMessageBox msg(this);
		msg.setText(e);
		msg.setInformativeText(tr("Failed to Select Device"));
		msg.setStandardButtons(QMessageBox::Ok);
		msg.setIcon(QMessageBox::Warning);
		msg.exec();
	}
}

void MainWindow::export_file(shared_ptr<OutputFormat> format,
	bool selection_only)
{
	using pv::dialogs::StoreProgress;

	// Make sure there's a view selected to pull the data from
	shared_ptr<pv::view::View> view = get_active_view();
	if (!view) {
		show_session_error(tr("No View Selected"), tr("Please click on the " \
				"view whose data you want to save and try again."));
		return;
	}

	// Stop any currently running capture session
	session_.stop_capture();

	QSettings settings;
	const QString dir = settings.value(SettingSaveDirectory).toString();

	std::pair<uint64_t, uint64_t> sample_range;

	// Selection only? Verify that the cursors are active and fetch their values
	if (selection_only) {
		if (!view->cursors()->enabled()) {
			show_session_error(tr("Missing Cursors"), tr("You need to set the " \
					"cursors before you can save the data enclosed by them " \
					"to a session file (e.g. using ALT-V - Show Cursors)."));
			return;
		}

		const double samplerate = session_.get_samplerate();

		const pv::util::Timestamp& start_time = view->cursors()->first()->time();
		const pv::util::Timestamp& end_time = view->cursors()->second()->time();

		const uint64_t start_sample =
			std::max((double)0, start_time.convert_to<double>() * samplerate);
		const uint64_t end_sample = end_time.convert_to<double>() * samplerate;

		sample_range = std::make_pair(start_sample, end_sample);
	} else {
		sample_range = std::make_pair(0, 0);
	}

	// Construct the filter
	const vector<string> exts = format->extensions();
	QString filter = tr("%1 files ").arg(
		QString::fromStdString(format->description()));

	if (exts.empty())
		filter += "(*.*)";
	else
		filter += QString("(*.%1);;%2 (*.*)").arg(
			QString::fromStdString(join(exts, ", *.")),
			tr("All Files"));

	// Show the file dialog
	const QString file_name = QFileDialog::getSaveFileName(
		this, tr("Save File"), dir, filter);

	if (file_name.isEmpty())
		return;

	const QString abs_path = QFileInfo(file_name).absolutePath();
	settings.setValue(SettingSaveDirectory, abs_path);

	// Show the options dialog
	map<string, Glib::VariantBase> options;
	if (!format->options().empty()) {
		dialogs::InputOutputOptions dlg(
			tr("Export %1").arg(QString::fromStdString(
				format->description())),
			format->options(), this);
		if (!dlg.exec())
			return;
		options = dlg.options();
	}

	StoreProgress *dlg = new StoreProgress(file_name, format, options,
		sample_range, session_, this);
	dlg->run();
}

void MainWindow::import_file(shared_ptr<InputFormat> format)
{
	assert(format);

	QSettings settings;
	const QString dir = settings.value(SettingOpenDirectory).toString();

	// Construct the filter
	const vector<string> exts = format->extensions();
	const QString filter = exts.empty() ? "" :
		tr("%1 files (*.%2)").arg(
			QString::fromStdString(format->description()),
			QString::fromStdString(join(exts, ", *.")));

	// Show the file dialog
	const QString file_name = QFileDialog::getOpenFileName(
		this, tr("Import File"), dir, tr(
			"%1 files (*.*);;All Files (*.*)").arg(
			QString::fromStdString(format->description())));

	if (file_name.isEmpty())
		return;

	// Show the options dialog
	map<string, Glib::VariantBase> options;
	if (!format->options().empty()) {
		dialogs::InputOutputOptions dlg(
			tr("Import %1").arg(QString::fromStdString(
				format->description())),
			format->options(), this);
		if (!dlg.exec())
			return;
		options = dlg.options();
	}

	load_file(file_name, format, options);

	const QString abs_path = QFileInfo(file_name).absolutePath();
	settings.setValue(SettingOpenDirectory, abs_path);
}

void MainWindow::setup_ui()
{
	setObjectName(QString::fromUtf8("MainWindow"));

	// Set the window icon
	QIcon icon;
	icon.addFile(QString(":/icons/sigrok-logo-notext.png"));
	setWindowIcon(icon);

	action_open_->setText(tr("&Open..."));
	action_open_->setIcon(QIcon::fromTheme("document-open",
		QIcon(":/icons/document-open.png")));
	action_open_->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_O));
	action_open_->setObjectName(QString::fromUtf8("actionOpen"));

	action_save_as_->setText(tr("&Save As..."));
	action_save_as_->setIcon(QIcon::fromTheme("document-save-as",
		QIcon(":/icons/document-save-as.png")));
	action_save_as_->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_S));
	action_save_as_->setObjectName(QString::fromUtf8("actionSaveAs"));

	action_save_selection_as_->setText(tr("Save Selected &Range As..."));
	action_save_selection_as_->setIcon(QIcon::fromTheme("document-save-as",
		QIcon(":/icons/document-save-as.png")));
	action_save_selection_as_->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_R));
	action_save_selection_as_->setObjectName(QString::fromUtf8("actionSaveSelectionAs"));

	widgets::ExportMenu *menu_file_export = new widgets::ExportMenu(this,
		device_manager_.context());
	menu_file_export->setTitle(tr("&Export"));
	connect(menu_file_export,
		SIGNAL(format_selected(std::shared_ptr<sigrok::OutputFormat>)),
		this, SLOT(export_file(std::shared_ptr<sigrok::OutputFormat>)));

	widgets::ImportMenu *menu_file_import = new widgets::ImportMenu(this,
		device_manager_.context());
	menu_file_import->setTitle(tr("&Import"));
	connect(menu_file_import,
		SIGNAL(format_selected(std::shared_ptr<sigrok::InputFormat>)),
		this, SLOT(import_file(std::shared_ptr<sigrok::InputFormat>)));

	action_connect_->setText(tr("&Connect to Device..."));
	action_connect_->setObjectName(QString::fromUtf8("actionConnect"));

	action_quit_->setText(tr("&Quit"));
	action_quit_->setIcon(QIcon::fromTheme("application-exit",
		QIcon(":/icons/application-exit.png")));
	action_quit_->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
	action_quit_->setObjectName(QString::fromUtf8("actionQuit"));

	action_view_zoom_in_->setText(tr("Zoom &In"));
	action_view_zoom_in_->setIcon(QIcon::fromTheme("zoom-in",
		QIcon(":/icons/zoom-in.png")));
	// simply using Qt::Key_Plus shows no + in the menu
	action_view_zoom_in_->setShortcut(QKeySequence::ZoomIn);
	action_view_zoom_in_->setObjectName(
		QString::fromUtf8("actionViewZoomIn"));

	action_view_zoom_out_->setText(tr("Zoom &Out"));
	action_view_zoom_out_->setIcon(QIcon::fromTheme("zoom-out",
		QIcon(":/icons/zoom-out.png")));
	action_view_zoom_out_->setShortcut(QKeySequence::ZoomOut);
	action_view_zoom_out_->setObjectName(
		QString::fromUtf8("actionViewZoomOut"));

	action_view_zoom_fit_->setCheckable(true);
	action_view_zoom_fit_->setText(tr("Zoom to &Fit"));
	action_view_zoom_fit_->setIcon(QIcon::fromTheme("zoom-fit",
		QIcon(":/icons/zoom-fit.png")));
	action_view_zoom_fit_->setShortcut(QKeySequence(Qt::Key_F));
	action_view_zoom_fit_->setObjectName(
		QString::fromUtf8("actionViewZoomFit"));

	action_view_zoom_one_to_one_->setText(tr("Zoom to O&ne-to-One"));
	action_view_zoom_one_to_one_->setIcon(QIcon::fromTheme("zoom-original",
		QIcon(":/icons/zoom-original.png")));
	action_view_zoom_one_to_one_->setShortcut(QKeySequence(Qt::Key_O));
	action_view_zoom_one_to_one_->setObjectName(
		QString::fromUtf8("actionViewZoomOneToOne"));

	action_view_sticky_scrolling_->setCheckable(true);
	action_view_sticky_scrolling_->setChecked(true);
	action_view_sticky_scrolling_->setShortcut(QKeySequence(Qt::Key_S));
	action_view_sticky_scrolling_->setObjectName(
		QString::fromUtf8("actionViewStickyScrolling"));
	action_view_sticky_scrolling_->setText(tr("&Sticky Scrolling"));

	action_view_coloured_bg_->setCheckable(true);
	action_view_coloured_bg_->setChecked(true);
	action_view_coloured_bg_->setShortcut(QKeySequence(Qt::Key_B));
	action_view_coloured_bg_->setObjectName(
		QString::fromUtf8("actionViewColouredBg"));
	action_view_coloured_bg_->setText(tr("Use &coloured backgrounds"));

	action_view_show_cursors_->setCheckable(true);
	action_view_show_cursors_->setIcon(QIcon::fromTheme("show-cursors",
		QIcon(":/icons/show-cursors.svg")));
	action_view_show_cursors_->setShortcut(QKeySequence(Qt::Key_C));
	action_view_show_cursors_->setObjectName(
		QString::fromUtf8("actionViewShowCursors"));
	action_view_show_cursors_->setText(tr("Show &Cursors"));

#ifdef ENABLE_DECODE
	menu_decoders_add_->setTitle(tr("&Add"));
	connect(menu_decoders_add_, SIGNAL(decoder_selected(srd_decoder*)),
		this, SLOT(add_decoder(srd_decoder*)));
#endif

	action_about_->setObjectName(QString::fromUtf8("actionAbout"));
	action_about_->setText(tr("&About..."));

	QMetaObject::connectSlotsByName(this);

	// Setup the toolbar
	main_bar_ = new toolbars::MainBar(session_, *this);

	// Set up the initial view
	add_view(tr("Untitled"), pv::view::TraceView, session_);

	// Populate the device list and select the initially selected device
	update_device_list();

	addToolBar(main_bar_);

	// Set the title
	setWindowTitle(tr("PulseView"));

	// Setup session_ events
	connect(&session_, SIGNAL(capture_state_changed(int)), this,
		SLOT(capture_state_changed(int)));
	connect(&session_, SIGNAL(device_selected()), this,
		SLOT(device_selected()));
}

void MainWindow::select_init_device()
{
	QSettings settings;
	map<string, string> dev_info;
	list<string> key_list;
	shared_ptr<devices::HardwareDevice> device;

	// Re-select last used device if possible but only if it's not demo
	settings.beginGroup("Device");
	key_list.push_back("vendor");
	key_list.push_back("model");
	key_list.push_back("version");
	key_list.push_back("serial_num");
	key_list.push_back("connection_id");

	for (string key : key_list) {
		const QString k = QString::fromStdString(key);
		if (!settings.contains(k))
			continue;

		const string value = settings.value(k).toString().toStdString();
		if (!value.empty())
			dev_info.insert(std::make_pair(key, value));
	}

	if (dev_info.count("model") > 0)
		if (dev_info.at("model").find("Demo device") == std::string::npos)
			device = device_manager_.find_device_from_info(dev_info);

	// When we can't find a device similar to the one we used last
	// time and there is at least one device aside from demo, use it
	if (!device) {
		for (shared_ptr<devices::HardwareDevice> dev : device_manager_.devices()) {
			dev_info = device_manager_.get_device_info(dev);

			if (dev_info.count("model") > 0)
				if (dev_info.at("model").find("Demo device") == std::string::npos) {
					device = dev;
					break;
				}
		}
	}

	select_device(device);
	update_device_list();

	settings.endGroup();
}

void MainWindow::load_init_file(const std::string &file_name,
	const std::string &format)
{
	shared_ptr<InputFormat> input_format;

	if (!format.empty()) {
		const map<string, shared_ptr<InputFormat> > formats =
			device_manager_.context()->input_formats();
		const auto iter = find_if(formats.begin(), formats.end(),
			[&](const pair<string, shared_ptr<InputFormat> > f) {
				return f.first == format; });
		if (iter == formats.end()) {
			cerr << "Unexpected input format: " << format << endl;
			return;
		}

		input_format = (*iter).second;
	}

	load_file(QString::fromStdString(file_name), input_format);
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

	settings.beginGroup("MainWindow");

	if (settings.contains("geometry")) {
		restoreGeometry(settings.value("geometry").toByteArray());
		restoreState(settings.value("state").toByteArray());
	} else
		resize(1000, 720);

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
	main_bar_->update_device_list();
}

void MainWindow::load_file(QString file_name,
	std::shared_ptr<sigrok::InputFormat> format,
	const std::map<std::string, Glib::VariantBase> &options)
{
	const QString errorMessage(
		QString("Failed to load file %1").arg(file_name));

	try {
		if (format)
			session_.set_device(shared_ptr<devices::Device>(
				new devices::InputFile(
					device_manager_.context(),
					file_name.toStdString(),
					format, options)));
		else
			session_.set_device(shared_ptr<devices::Device>(
				new devices::SessionFile(
					device_manager_.context(),
					file_name.toStdString())));
	} catch (Error e) {
		show_session_error(tr("Failed to load ") + file_name, e.what());
		session_.set_default_device();
		update_device_list();
		return;
	}

	update_device_list();

	session_.start_capture([&, errorMessage](QString infoMessage) {
		session_error(errorMessage, infoMessage); });
}

void MainWindow::closeEvent(QCloseEvent *event)
{
	save_ui_settings();
	event->accept();
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
	if (event->key() == Qt::Key_Alt) {
		menuBar()->setHidden(!menuBar()->isHidden());
		menuBar()->setFocus();
	}
	QMainWindow::keyReleaseEvent(event);
}

QMenu* MainWindow::createPopupMenu()
{
	return nullptr;
}

bool MainWindow::restoreState(const QByteArray &state, int version)
{
	(void)state;
	(void)version;

	// Do nothing. We don't want Qt to handle this, or else it
	// will try to restore all the dock widgets and create havoc.

	return false;
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
	export_file(device_manager_.context()->output_formats()["srzip"]);
}

void MainWindow::on_actionSaveSelectionAs_triggered()
{
	export_file(device_manager_.context()->output_formats()["srzip"], true);
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
	shared_ptr<pv::view::View> view = get_active_view();
	if (view)
		view->zoom(1);
}

void MainWindow::on_actionViewZoomOut_triggered()
{
	shared_ptr<pv::view::View> view = get_active_view();
	if (view)
		view->zoom(-1);
}

void MainWindow::on_actionViewZoomFit_triggered()
{
	shared_ptr<pv::view::View> view = get_active_view();
	if (view)
		view->zoom_fit(action_view_zoom_fit_->isChecked());
}

void MainWindow::on_actionViewZoomOneToOne_triggered()
{
	shared_ptr<pv::view::View> view = get_active_view();
	if (view)
		view->zoom_one_to_one();
}

void MainWindow::on_actionViewStickyScrolling_triggered()
{
	shared_ptr<pv::view::View> view = get_active_view();
	if (view)
		view->enable_sticky_scrolling(action_view_sticky_scrolling_->isChecked());
}

void MainWindow::on_actionViewColouredBg_triggered()
{
	shared_ptr<pv::view::View> view = get_active_view();
	if (view)
		view->enable_coloured_bg(action_view_coloured_bg_->isChecked());
}

void MainWindow::on_actionViewShowCursors_triggered()
{
	shared_ptr<pv::view::View> view = get_active_view();
	if (!view)
		return;

	const bool show = !view->cursors_shown();
	if (show)
		view->centre_cursors();

	view->show_cursors(show);
}

void MainWindow::on_actionAbout_triggered()
{
	dialogs::About dlg(device_manager_.context(), this);
	dlg.exec();
}

void MainWindow::sticky_scrolling_changed(bool state)
{
	action_view_sticky_scrolling_->setChecked(state);
}

void MainWindow::always_zoom_to_fit_changed(bool state)
{
	action_view_zoom_fit_->setChecked(state);
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
	main_bar_->set_capture_state((pv::Session::capture_state)state);
}

void MainWindow::device_selected()
{
	// Set the title to include the device/file name
	const shared_ptr<devices::Device> device = session_.device();

	if (!device) {
		main_bar_->reset_device_selector();
		return;
	}

	const string display_name = device->display_name(device_manager_);
	setWindowTitle(tr("%1 - PulseView").arg(display_name.c_str()));
}

} // namespace pv
