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

#ifdef ENABLE_DECODE
#include <libsigrokdecode/libsigrokdecode.h>
#endif

#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <iterator>

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDebug>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QSettings>
#include <QShortcut>
#include <QWidget>

#include "mainwindow.hpp"

#include "devicemanager.hpp"
#include "devices/hardwaredevice.hpp"
#include "dialogs/settings.hpp"
#include "globalsettings.hpp"
#include "toolbars/mainbar.hpp"
#include "util.hpp"
#include "views/trace/view.hpp"
#include "views/trace/standardbar.hpp"

#ifdef ENABLE_DECODE
#include "subwindows/decoder_selector/subwindow.hpp"
#endif

#include <libsigrokcxx/libsigrokcxx.hpp>

using std::dynamic_pointer_cast;
using std::make_shared;
using std::shared_ptr;
using std::string;

namespace pv {

namespace view {
class ViewItem;
}

using toolbars::MainBar;

const QString MainWindow::WindowTitle = tr("PulseView");

MainWindow::MainWindow(DeviceManager &device_manager, QWidget *parent) :
	QMainWindow(parent),
	device_manager_(device_manager),
	session_selector_(this),
	session_state_mapper_(this),
	icon_red_(":/icons/status-red.svg"),
	icon_green_(":/icons/status-green.svg"),
	icon_grey_(":/icons/status-grey.svg")
{
	GlobalSettings::add_change_handler(this);

	setup_ui();
	restore_ui_settings();
}

MainWindow::~MainWindow()
{
	GlobalSettings::remove_change_handler(this);

	// Make sure we no longer hold any shared pointers to widgets after the
	// destructor finishes (goes for sessions and sub windows alike)

	while (!sessions_.empty())
		remove_session(sessions_.front());

	sub_windows_.clear();
}

void MainWindow::show_session_error(const QString text, const QString info_text)
{
	// TODO Emulate noquote()
	qDebug() << "Notifying user of session error:" << info_text;

	QMessageBox msg;
	msg.setText(text + "\n\n" + info_text);
	msg.setStandardButtons(QMessageBox::Ok);
	msg.setIcon(QMessageBox::Warning);
	msg.exec();
}

shared_ptr<views::ViewBase> MainWindow::get_active_view() const
{
	// If there's only one view, use it...
	if (view_docks_.size() == 1)
		return view_docks_.begin()->second;

	// ...otherwise find the dock widget the widget with focus is contained in
	QObject *w = QApplication::focusWidget();
	QDockWidget *dock = nullptr;

	while (w) {
		dock = qobject_cast<QDockWidget*>(w);
		if (dock)
			break;
		w = w->parent();
	}

	// Get the view contained in the dock widget
	for (auto& entry : view_docks_)
		if (entry.first == dock)
			return entry.second;

	return nullptr;
}

shared_ptr<views::ViewBase> MainWindow::add_view(const QString &title,
	views::ViewType type, Session &session)
{
	GlobalSettings settings;
	shared_ptr<views::ViewBase> v;

	QMainWindow *main_window = nullptr;
	for (auto& entry : session_windows_)
		if (entry.first.get() == &session)
			main_window = entry.second;

	assert(main_window);

	shared_ptr<MainBar> main_bar = session.main_bar();

	QDockWidget* dock = new QDockWidget(title, main_window);
	dock->setObjectName(title);
	main_window->addDockWidget(Qt::TopDockWidgetArea, dock);

	// Insert a QMainWindow into the dock widget to allow for a tool bar
	QMainWindow *dock_main = new QMainWindow(dock);
	dock_main->setWindowFlags(Qt::Widget);  // Remove Qt::Window flag

	if (type == views::ViewTypeTrace)
		// This view will be the main view if there's no main bar yet
		v = make_shared<views::trace::View>(session,
			(main_bar ? false : true), dock_main);

	if (!v)
		return nullptr;

	view_docks_[dock] = v;
	session.register_view(v);

	dock_main->setCentralWidget(v.get());
	dock->setWidget(dock_main);

	dock->setContextMenuPolicy(Qt::PreventContextMenu);
	dock->setFeatures(QDockWidget::DockWidgetMovable |
		QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);

	QAbstractButton *close_btn =
		dock->findChildren<QAbstractButton*>("qt_dockwidget_closebutton")  // clazy:exclude=detaching-temporary
			.front();

	connect(close_btn, SIGNAL(clicked(bool)),
		this, SLOT(on_view_close_clicked()));

	connect(&session, SIGNAL(trigger_event(int, util::Timestamp)),
		qobject_cast<views::ViewBase*>(v.get()),
		SLOT(trigger_event(int, util::Timestamp)));

	if (type == views::ViewTypeTrace) {
		views::trace::View *tv =
			qobject_cast<views::trace::View*>(v.get());

		tv->enable_colored_bg(settings.value(GlobalSettings::Key_View_ColoredBG).toBool());
		tv->enable_show_sampling_points(settings.value(GlobalSettings::Key_View_ShowSamplingPoints).toBool());
		tv->enable_show_analog_minor_grid(settings.value(GlobalSettings::Key_View_ShowAnalogMinorGrid).toBool());

		if (!main_bar) {
			/* Initial view, create the main bar */
			main_bar = make_shared<MainBar>(session, this, tv);
			dock_main->addToolBar(main_bar.get());
			session.set_main_bar(main_bar);

			connect(main_bar.get(), SIGNAL(new_view(Session*)),
				this, SLOT(on_new_view(Session*)));
			connect(main_bar.get(), SIGNAL(show_decoder_selector(Session*)),
				this, SLOT(on_show_decoder_selector(Session*)));

			main_bar->action_view_show_cursors()->setChecked(tv->cursors_shown());

			/* For the main view we need to prevent the dock widget from
			 * closing itself when its close button is clicked. This is
			 * so we can confirm with the user first. Regular views don't
			 * need this */
			close_btn->disconnect(SIGNAL(clicked()), dock, SLOT(close()));
		} else {
			/* Additional view, create a standard bar */
			pv::views::trace::StandardBar *standard_bar =
				new pv::views::trace::StandardBar(session, this, tv);
			dock_main->addToolBar(standard_bar);

			standard_bar->action_view_show_cursors()->setChecked(tv->cursors_shown());
		}
	}

	return v;
}

void MainWindow::remove_view(shared_ptr<views::ViewBase> view)
{
	for (shared_ptr<Session> session : sessions_) {
		if (!session->has_view(view))
			continue;

		// Find the dock the view is contained in and remove it
		for (auto& entry : view_docks_)
			if (entry.second == view) {
				// Remove the view from the session
				session->deregister_view(view);

				// Remove the view from its parent; otherwise, Qt will
				// call deleteLater() on it, which causes a double free
				// since the shared_ptr in view_docks_ doesn't know
				// that Qt keeps a pointer to the view around
				view->setParent(nullptr);

				// Delete the view's dock widget and all widgets inside it
				entry.first->deleteLater();

				// Remove the dock widget from the list and stop iterating
				view_docks_.erase(entry.first);
				break;
			}
	}
}

shared_ptr<subwindows::SubWindowBase> MainWindow::add_subwindow(
	subwindows::SubWindowType type, Session &session)
{
	GlobalSettings settings;
	shared_ptr<subwindows::SubWindowBase> v;

	QMainWindow *main_window = nullptr;
	for (auto entry : session_windows_)
		if (entry.first.get() == &session)
			main_window = entry.second;

	assert(main_window);

	QString title = "";

	switch (type) {
#ifdef ENABLE_DECODE
		case subwindows::SubWindowTypeDecoderSelector:
			title = tr("Decoder Selector");
			break;
#endif
		default:
			break;
	}

	QDockWidget* dock = new QDockWidget(title, main_window);
	dock->setObjectName(title);
	main_window->addDockWidget(Qt::TopDockWidgetArea, dock);

	// Insert a QMainWindow into the dock widget to allow for a tool bar
	QMainWindow *dock_main = new QMainWindow(dock);
	dock_main->setWindowFlags(Qt::Widget);  // Remove Qt::Window flag

#ifdef ENABLE_DECODE
	if (type == subwindows::SubWindowTypeDecoderSelector)
		v = make_shared<subwindows::decoder_selector::SubWindow>(session, dock_main);
#endif

	if (!v)
		return nullptr;

	sub_windows_[dock] = v;
	dock_main->setCentralWidget(v.get());
	dock->setWidget(dock_main);

	dock->setContextMenuPolicy(Qt::PreventContextMenu);
	dock->setFeatures(QDockWidget::DockWidgetMovable |
		QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);

	QAbstractButton *close_btn =
		dock->findChildren<QAbstractButton*>
			("qt_dockwidget_closebutton").front();

	connect(close_btn, SIGNAL(clicked(bool)),
		this, SLOT(on_sub_window_close_clicked()));

	if (v->has_toolbar())
		dock_main->addToolBar(v->create_toolbar(dock_main));

	if (v->minimum_width() > 0)
		dock->setMinimumSize(v->minimum_width(), 0);

	return v;
}

shared_ptr<Session> MainWindow::add_session()
{
	static int last_session_id = 1;
	QString name = tr("Session %1").arg(last_session_id++);

	shared_ptr<Session> session = make_shared<Session>(device_manager_, name);

	connect(session.get(), SIGNAL(add_view(const QString&, views::ViewType, Session*)),
		this, SLOT(on_add_view(const QString&, views::ViewType, Session*)));
	connect(session.get(), SIGNAL(name_changed()),
		this, SLOT(on_session_name_changed()));
	session_state_mapper_.setMapping(session.get(), session.get());
	connect(session.get(), SIGNAL(capture_state_changed(int)),
		&session_state_mapper_, SLOT(map()));

	sessions_.push_back(session);

	QMainWindow *window = new QMainWindow();
	window->setWindowFlags(Qt::Widget);  // Remove Qt::Window flag
	session_windows_[session] = window;

	int index = session_selector_.addTab(window, name);
	session_selector_.setCurrentIndex(index);
	last_focused_session_ = session;

	window->setDockNestingEnabled(true);

	shared_ptr<views::ViewBase> main_view =
		add_view(name, views::ViewTypeTrace, *session);

	return session;
}

void MainWindow::remove_session(shared_ptr<Session> session)
{
	// Determine the height of the button before it collapses
	int h = new_session_button_->height();

	// Stop capture while the session still exists so that the UI can be
	// updated in case we're currently running. If so, this will schedule a
	// call to our on_capture_state_changed() slot for the next run of the
	// event loop. We need to have this executed immediately or else it will
	// be dismissed since the session object will be deleted by the time we
	// leave this method and the event loop gets a chance to run again.
	session->stop_capture();
	QApplication::processEvents();

	for (const shared_ptr<views::ViewBase>& view : session->views())
		remove_view(view);

	QMainWindow *window = session_windows_.at(session);
	session_selector_.removeTab(session_selector_.indexOf(window));

	session_windows_.erase(session);

	if (last_focused_session_ == session)
		last_focused_session_.reset();

	// Remove the session from our list of sessions (which also destroys it)
	sessions_.remove_if([&](shared_ptr<Session> s) {
		return s == session; });

	if (sessions_.empty()) {
		// When there are no more tabs, the height of the QTabWidget
		// drops to zero. We must prevent this to keep the static
		// widgets visible
		for (QWidget *w : static_tab_widget_->findChildren<QWidget*>())  // clazy:exclude=range-loop
			w->setMinimumHeight(h);

		int margin = static_tab_widget_->layout()->contentsMargins().bottom();
		static_tab_widget_->setMinimumHeight(h + 2 * margin);
		session_selector_.setMinimumHeight(h + 2 * margin);

		// Update the window title if there is no view left to
		// generate focus change events
		setWindowTitle(WindowTitle);
	}
}

void MainWindow::add_session_with_file(string open_file_name,
	string open_file_format, string open_setup_file_name)
{
	shared_ptr<Session> session = add_session();
	session->load_init_file(open_file_name, open_file_format, open_setup_file_name);
}

void MainWindow::add_default_session()
{
	// Only add the default session if there would be no session otherwise
	if (sessions_.size() > 0)
		return;

	shared_ptr<Session> session = add_session();

	// Check the list of available devices. Prefer the one that was
	// found with user supplied scan specs (if applicable). Then try
	// one of the auto detected devices that are not the demo device.
	// Pick demo in the absence of "genuine" hardware devices.
	shared_ptr<devices::HardwareDevice> user_device, other_device, demo_device;
	for (const shared_ptr<devices::HardwareDevice>& dev : device_manager_.devices()) {
		if (dev == device_manager_.user_spec_device()) {
			user_device = dev;
		} else if (dev->hardware_device()->driver()->name() == "demo") {
			demo_device = dev;
		} else {
			other_device = dev;
		}
	}
	if (user_device)
		session->select_device(user_device);
	else if (other_device)
		session->select_device(other_device);
	else
		session->select_device(demo_device);
}

void MainWindow::save_sessions()
{
	QSettings settings;
	int id = 0;

	for (shared_ptr<Session>& session : sessions_) {
		// Ignore sessions using the demo device or no device at all
		if (session->device()) {
			shared_ptr<devices::HardwareDevice> device =
				dynamic_pointer_cast< devices::HardwareDevice >
				(session->device());

			if (device &&
				device->hardware_device()->driver()->name() == "demo")
				continue;

			settings.beginGroup("Session" + QString::number(id++));
			settings.remove("");  // Remove all keys in this group
			session->save_settings(settings);
			settings.endGroup();
		}
	}

	settings.setValue("sessions", id);
}

void MainWindow::restore_sessions()
{
	QSettings settings;
	int i, session_count;

	session_count = settings.value("sessions", 0).toInt();

	for (i = 0; i < session_count; i++) {
		settings.beginGroup("Session" + QString::number(i));
		shared_ptr<Session> session = add_session();
		session->restore_settings(settings);
		settings.endGroup();
	}
}

void MainWindow::on_setting_changed(const QString &key, const QVariant &value)
{
	if (key == GlobalSettings::Key_View_ColoredBG)
		on_settingViewColoredBg_changed(value);

	if (key == GlobalSettings::Key_View_ShowSamplingPoints)
		on_settingViewShowSamplingPoints_changed(value);

	if (key == GlobalSettings::Key_View_ShowAnalogMinorGrid)
		on_settingViewShowAnalogMinorGrid_changed(value);
}

void MainWindow::setup_ui()
{
	setObjectName(QString::fromUtf8("MainWindow"));

	setCentralWidget(&session_selector_);

	// Set the window icon
	QIcon icon;
	icon.addFile(QString(":/icons/pulseview.png"));
	setWindowIcon(icon);

	view_sticky_scrolling_shortcut_ = new QShortcut(QKeySequence(Qt::Key_S), this, SLOT(on_view_sticky_scrolling_shortcut()));
	view_sticky_scrolling_shortcut_->setAutoRepeat(false);

	view_show_sampling_points_shortcut_ = new QShortcut(QKeySequence(Qt::Key_Period), this, SLOT(on_view_show_sampling_points_shortcut()));
	view_show_sampling_points_shortcut_->setAutoRepeat(false);

	view_show_analog_minor_grid_shortcut_ = new QShortcut(QKeySequence(Qt::Key_G), this, SLOT(on_view_show_analog_minor_grid_shortcut()));
	view_show_analog_minor_grid_shortcut_->setAutoRepeat(false);

	view_colored_bg_shortcut_ = new QShortcut(QKeySequence(Qt::Key_B), this, SLOT(on_view_colored_bg_shortcut()));
	view_colored_bg_shortcut_->setAutoRepeat(false);

	zoom_in_shortcut_ = new QShortcut(QKeySequence(Qt::Key_Plus), this, SLOT(on_zoom_in_shortcut_triggered()));
	zoom_in_shortcut_->setAutoRepeat(false);

	zoom_out_shortcut_ = new QShortcut(QKeySequence(Qt::Key_Minus), this, SLOT(on_zoom_out_shortcut_triggered()));
	zoom_out_shortcut_->setAutoRepeat(false);

	grab_ruler_left_shortcut_ = new QShortcut(QKeySequence(Qt::Key_0), this);
	connect(grab_ruler_left_shortcut_, &QShortcut::activated, this, [=]{on_grab_ruler(true);});
	grab_ruler_left_shortcut_->setAutoRepeat(false);

	grab_ruler_right_shortcut_ = new QShortcut(QKeySequence(Qt::Key_1), this);
	connect(grab_ruler_right_shortcut_, &QShortcut::activated, this, [=]{on_grab_ruler(false);});
	grab_ruler_right_shortcut_->setAutoRepeat(false);

	exit_grab_shortcut_ = new QShortcut(QKeySequence(Qt::Key_Escape), this, SLOT(on_cancel_grab()));
	exit_grab_shortcut_->setAutoRepeat(false);

	// Set up the tab area
	new_session_button_ = new QToolButton();
	new_session_button_->setIcon(QIcon::fromTheme("document-new",
		QIcon(":/icons/document-new.png")));
	new_session_button_->setToolTip(tr("Create New Session"));
	new_session_button_->setAutoRaise(true);

	run_stop_button_ = new QToolButton();
	run_stop_button_->setAutoRaise(true);
	run_stop_button_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	run_stop_button_->setToolTip(tr("Start/Stop Acquisition"));

	run_stop_shortcut_ = new QShortcut(QKeySequence(Qt::Key_Space), run_stop_button_, SLOT(click()));
	run_stop_shortcut_->setAutoRepeat(false);

	settings_button_ = new QToolButton();
	settings_button_->setIcon(QIcon::fromTheme("preferences-system",
		QIcon(":/icons/preferences-system.png")));
	settings_button_->setToolTip(tr("Settings"));
	settings_button_->setAutoRaise(true);

	QFrame *separator1 = new QFrame();
	separator1->setFrameStyle(QFrame::VLine | QFrame::Raised);
	QFrame *separator2 = new QFrame();
	separator2->setFrameStyle(QFrame::VLine | QFrame::Raised);

	QHBoxLayout* layout = new QHBoxLayout();
	layout->setContentsMargins(2, 2, 2, 2);
	layout->addWidget(new_session_button_);
	layout->addWidget(separator1);
	layout->addWidget(run_stop_button_);
	layout->addWidget(separator2);
	layout->addWidget(settings_button_);

	static_tab_widget_ = new QWidget();
	static_tab_widget_->setLayout(layout);

	session_selector_.setCornerWidget(static_tab_widget_, Qt::TopLeftCorner);
	session_selector_.setTabsClosable(true);

	close_application_shortcut_ = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q), this, SLOT(close()));
	close_application_shortcut_->setAutoRepeat(false);

	close_current_tab_shortcut_ = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_W), this, SLOT(on_close_current_tab()));

	connect(new_session_button_, SIGNAL(clicked(bool)),
		this, SLOT(on_new_session_clicked()));
	connect(run_stop_button_, SIGNAL(clicked(bool)),
		this, SLOT(on_run_stop_clicked()));
	connect(&session_state_mapper_, SIGNAL(mapped(QObject*)),
		this, SLOT(on_capture_state_changed(QObject*)));
	connect(settings_button_, SIGNAL(clicked(bool)),
		this, SLOT(on_settings_clicked()));

	connect(&session_selector_, SIGNAL(tabCloseRequested(int)),
		this, SLOT(on_tab_close_requested(int)));
	connect(&session_selector_, SIGNAL(currentChanged(int)),
		this, SLOT(on_tab_changed(int)));


	connect(static_cast<QApplication *>(QCoreApplication::instance()),
		SIGNAL(focusChanged(QWidget*, QWidget*)),
		this, SLOT(on_focus_changed()));
}

void MainWindow::save_ui_settings()
{
	QSettings settings;

	settings.beginGroup("MainWindow");
	settings.setValue("state", saveState());
	settings.setValue("geometry", saveGeometry());
	settings.endGroup();
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

void MainWindow::zoom_current_view(double steps)
{
	shared_ptr<Session> session = get_tab_session(session_selector_.currentIndex());

	if (!session)
		return;

	shared_ptr<views::ViewBase> v = session.get()->main_view();
	views::trace::View *tv =
		qobject_cast<views::trace::View*>(v.get());
	tv->zoom(steps);
}

shared_ptr<Session> MainWindow::get_tab_session(int index) const
{
	// Find the session that belongs to the tab's main window
	for (auto& entry : session_windows_)
		if (entry.second == session_selector_.widget(index))
			return entry.first;

	return nullptr;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
	bool data_saved = true;

	for (auto& entry : session_windows_)
		if (!entry.first->data_saved())
			data_saved = false;

	if (!data_saved && (QMessageBox::question(this, tr("Confirmation"),
		tr("There is unsaved data. Close anyway?"),
		QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)) {
		event->ignore();
	} else {
		save_ui_settings();
		save_sessions();
		event->accept();
	}
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

void MainWindow::on_add_view(const QString &title, views::ViewType type,
	Session *session)
{
	// We get a pointer and need a reference
	for (shared_ptr<Session>& s : sessions_)
		if (s.get() == session)
			add_view(title, type, *s);
}

void MainWindow::on_focus_changed()
{
	shared_ptr<views::ViewBase> view = get_active_view();

	if (view) {
		for (shared_ptr<Session> session : sessions_) {
			if (session->has_view(view)) {
				if (session != last_focused_session_) {
					// Activate correct tab if necessary
					shared_ptr<Session> tab_session = get_tab_session(
						session_selector_.currentIndex());
					if (tab_session != session)
						session_selector_.setCurrentWidget(
							session_windows_.at(session));

					on_focused_session_changed(session);
				}

				break;
			}
		}
	}

	if (sessions_.empty())
		setWindowTitle(WindowTitle);
}

void MainWindow::on_focused_session_changed(shared_ptr<Session> session)
{
	last_focused_session_ = session;

	setWindowTitle(session->name() + " - " + WindowTitle);

	// Update the state of the run/stop button, too
	on_capture_state_changed(session.get());
}

void MainWindow::on_new_session_clicked()
{
	add_session();
}

void MainWindow::on_run_stop_clicked()
{
	shared_ptr<Session> session = last_focused_session_;

	if (!session)
		return;

	switch (session->get_capture_state()) {
	case Session::Stopped:
		session->start_capture([&](QString message) {
			show_session_error("Capture failed", message); });
		break;
	case Session::AwaitingTrigger:
	case Session::Running:
		session->stop_capture();
		break;
	}
}

void MainWindow::on_settings_clicked()
{
	dialogs::Settings dlg(device_manager_);
	dlg.exec();
}

void MainWindow::on_session_name_changed()
{
	// Update the corresponding dock widget's name(s)
	Session *session = qobject_cast<Session*>(QObject::sender());
	assert(session);

	for (const shared_ptr<views::ViewBase>& view : session->views()) {
		// Get the dock that contains the view
		for (auto& entry : view_docks_)
			if (entry.second == view) {
				entry.first->setObjectName(session->name());
				entry.first->setWindowTitle(session->name());
			}
	}

	// Update the tab widget by finding the main window and the tab from that
	for (auto& entry : session_windows_)
		if (entry.first.get() == session) {
			QMainWindow *window = entry.second;
			const int index = session_selector_.indexOf(window);
			session_selector_.setTabText(index, session->name());
		}

	// Refresh window title if the affected session has focus
	if (session == last_focused_session_.get())
		setWindowTitle(session->name() + " - " + WindowTitle);
}

void MainWindow::on_capture_state_changed(QObject *obj)
{
	Session *caller = qobject_cast<Session*>(obj);

	// Ignore if caller is not the currently focused session
	// unless there is only one session
	if ((sessions_.size() > 1) && (caller != last_focused_session_.get()))
		return;

	int state = caller->get_capture_state();

	const QIcon *icons[] = {&icon_grey_, &icon_red_, &icon_green_};
	run_stop_button_->setIcon(*icons[state]);
	run_stop_button_->setText((state == pv::Session::Stopped) ?
		tr("Run") : tr("Stop"));
}

void MainWindow::on_new_view(Session *session)
{
	// We get a pointer and need a reference
	for (shared_ptr<Session>& s : sessions_)
		if (s.get() == session)
			add_view(session->name(), views::ViewTypeTrace, *s);
}

void MainWindow::on_view_close_clicked()
{
	// Find the dock widget that contains the close button that was clicked
	QObject *w = QObject::sender();
	QDockWidget *dock = nullptr;

	while (w) {
	    dock = qobject_cast<QDockWidget*>(w);
	    if (dock)
	        break;
	    w = w->parent();
	}

	// Get the view contained in the dock widget
	shared_ptr<views::ViewBase> view;

	for (auto& entry : view_docks_)
		if (entry.first == dock)
			view = entry.second;

	// Deregister the view
	for (shared_ptr<Session> session : sessions_) {
		if (!session->has_view(view))
			continue;

		// Also destroy the entire session if its main view is closing...
		if (view == session->main_view()) {
			// ...but only if data is saved or the user confirms closing
			if (session->data_saved() || (QMessageBox::question(this, tr("Confirmation"),
				tr("This session contains unsaved data. Close it anyway?"),
				QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes))
				remove_session(session);
			break;
		} else
			// All other views can be closed at any time as no data will be lost
			remove_view(view);
	}
}

void MainWindow::on_tab_changed(int index)
{
	shared_ptr<Session> session = get_tab_session(index);

	if (session)
		on_focused_session_changed(session);
}

void MainWindow::on_tab_close_requested(int index)
{
	shared_ptr<Session> session = get_tab_session(index);

	if (!session)
		return;

	if (session->data_saved() || (QMessageBox::question(this, tr("Confirmation"),
		tr("This session contains unsaved data. Close it anyway?"),
		QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes))
		remove_session(session);
}

void MainWindow::on_show_decoder_selector(Session *session)
{
#ifdef ENABLE_DECODE
	// Close dock widget if it's already showing and return
	for (auto entry : sub_windows_) {
		QDockWidget* dock = entry.first;
		if (dynamic_pointer_cast<subwindows::decoder_selector::SubWindow>(entry.second)) {
			sub_windows_.erase(dock);
			dock->close();
			return;
		}
	}

	// We get a pointer and need a reference
	for (shared_ptr<Session> s : sessions_)
		if (s.get() == session)
			add_subwindow(subwindows::SubWindowTypeDecoderSelector, *s);
#endif
}

void MainWindow::on_sub_window_close_clicked()
{
	// Find the dock widget that contains the close button that was clicked
	QObject *w = QObject::sender();
	QDockWidget *dock = nullptr;

	while (w) {
	    dock = qobject_cast<QDockWidget*>(w);
	    if (dock)
	        break;
	    w = w->parent();
	}

	sub_windows_.erase(dock);
	dock->close();
}

void MainWindow::on_view_colored_bg_shortcut()
{
	GlobalSettings settings;

	bool state = settings.value(GlobalSettings::Key_View_ColoredBG).toBool();
	settings.setValue(GlobalSettings::Key_View_ColoredBG, !state);
}

void MainWindow::on_view_sticky_scrolling_shortcut()
{
	GlobalSettings settings;

	bool state = settings.value(GlobalSettings::Key_View_StickyScrolling).toBool();
	settings.setValue(GlobalSettings::Key_View_StickyScrolling, !state);
}

void MainWindow::on_view_show_sampling_points_shortcut()
{
	GlobalSettings settings;

	bool state = settings.value(GlobalSettings::Key_View_ShowSamplingPoints).toBool();
	settings.setValue(GlobalSettings::Key_View_ShowSamplingPoints, !state);
}

void MainWindow::on_view_show_analog_minor_grid_shortcut()
{
	GlobalSettings settings;

	bool state = settings.value(GlobalSettings::Key_View_ShowAnalogMinorGrid).toBool();
	settings.setValue(GlobalSettings::Key_View_ShowAnalogMinorGrid, !state);
}

void MainWindow::on_settingViewColoredBg_changed(const QVariant new_value)
{
	bool state = new_value.toBool();

	for (auto& entry : view_docks_) {
		shared_ptr<views::ViewBase> viewbase = entry.second;

		// Only trace views have this setting
		views::trace::View* view =
				qobject_cast<views::trace::View*>(viewbase.get());
		if (view)
			view->enable_colored_bg(state);
	}
}

void MainWindow::on_settingViewShowSamplingPoints_changed(const QVariant new_value)
{
	bool state = new_value.toBool();

	for (auto& entry : view_docks_) {
		shared_ptr<views::ViewBase> viewbase = entry.second;

		// Only trace views have this setting
		views::trace::View* view =
				qobject_cast<views::trace::View*>(viewbase.get());
		if (view)
			view->enable_show_sampling_points(state);
	}
}

void MainWindow::on_settingViewShowAnalogMinorGrid_changed(const QVariant new_value)
{
	bool state = new_value.toBool();

	for (auto& entry : view_docks_) {
		shared_ptr<views::ViewBase> viewbase = entry.second;

		// Only trace views have this setting
		views::trace::View* view =
				qobject_cast<views::trace::View*>(viewbase.get());
		if (view)
			view->enable_show_analog_minor_grid(state);
	}
}

void MainWindow::on_zoom_out_shortcut_triggered()
{
	zoom_current_view(-1);
}

void MainWindow::on_zoom_in_shortcut_triggered()
{
	zoom_current_view(1);
}

void MainWindow::on_grab_ruler(bool first)
{
	shared_ptr<Session> session = get_tab_session(session_selector_.currentIndex());

	if (!session)
		return;
	shared_ptr<views::ViewBase> v = session.get()->main_view();
	views::trace::View *tv =
			qobject_cast<views::trace::View*>(v.get());
	if (tv->cursors_shown()) {
		if (first)
			tv->set_grabbed_widget(tv->cursors()->first().get());
		else
			tv->set_grabbed_widget(tv->cursors()->second().get());
	}
}

void MainWindow::on_cancel_grab()
{
	shared_ptr<Session> session = get_tab_session(session_selector_.currentIndex());

	if (!session)
		return;

	shared_ptr<views::ViewBase> v = session.get()->main_view();
	views::trace::View *tv =
		qobject_cast<views::trace::View*>(v.get());
	tv->clear_grabbed_widget();
}

void MainWindow::on_close_current_tab()
{
	int tab = session_selector_.currentIndex();

	on_tab_close_requested(tab);
}

} // namespace pv
