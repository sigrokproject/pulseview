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
#include <QCloseEvent>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QSettings>
#include <QWidget>

#include "mainwindow.hpp"

#include "devicemanager.hpp"
#include "util.hpp"
#include "devices/hardwaredevice.hpp"
#include "dialogs/about.hpp"
#include "toolbars/mainbar.hpp"
#include "view/view.hpp"

#include <stdint.h>
#include <stdarg.h>
#include <libsigrokcxx/libsigrokcxx.hpp>

using std::dynamic_pointer_cast;
using std::list;
using std::make_shared;
using std::map;
using std::shared_ptr;
using std::string;

namespace pv {

namespace view {
class ViewItem;
}

using toolbars::MainBar;

const QString MainWindow::WindowTitle = tr("PulseView");

MainWindow::MainWindow(DeviceManager &device_manager,
	string open_file_name, string open_file_format,
	QWidget *parent) :
	QMainWindow(parent),
	device_manager_(device_manager),
	session_selector_(this),
	session_state_mapper_(this),
	action_view_sticky_scrolling_(new QAction(this)),
	action_view_coloured_bg_(new QAction(this)),
	action_about_(new QAction(this)),
	icon_red_(":/icons/status-red.svg"),
	icon_green_(":/icons/status-green.svg"),
	icon_grey_(":/icons/status-grey.svg")
{
	qRegisterMetaType<util::Timestamp>("util::Timestamp");

	setup_ui();
	restore_ui_settings();

	if (!open_file_name.empty()) {
		shared_ptr<Session> session = add_session();
		session->main_bar()->load_init_file(open_file_name, open_file_format);
	}

	// Add empty default session if there aren't any sessions
	if (sessions_.size() == 0) {
		shared_ptr<Session> session = add_session();

		map<string, string> dev_info;
		shared_ptr<devices::HardwareDevice> other_device, demo_device;

		// Use any available device that's not demo
		for (shared_ptr<devices::HardwareDevice> dev : device_manager_.devices()) {
			if (dev->hardware_device()->driver()->name() == "demo") {
				demo_device = dev;
			} else {
				other_device = dev;
			}
		}

		// ...and if there isn't any, just use demo then
		session->main_bar()->select_device(other_device ?
			other_device : demo_device);
	}
}

MainWindow::~MainWindow()
{
	while (!sessions_.empty())
		remove_session(sessions_.front());
}

QAction* MainWindow::action_view_sticky_scrolling() const
{
	return action_view_sticky_scrolling_;
}

QAction* MainWindow::action_view_coloured_bg() const
{
	return action_view_coloured_bg_;
}

QAction* MainWindow::action_about() const
{
	return action_about_;
}

shared_ptr<views::ViewBase> MainWindow::get_active_view() const
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
		if (entry.first == dock)
			return entry.second;

	return nullptr;
}

shared_ptr<views::ViewBase> MainWindow::add_view(const QString &title,
	views::ViewType type, Session &session)
{
	QMainWindow *main_window;
	for (auto entry : session_windows_)
		if (entry.first.get() == &session)
			main_window = entry.second;

	assert(main_window);

	if (type == views::ViewTypeTrace) {
		QDockWidget* dock = new QDockWidget(title, main_window);
		dock->setObjectName(title);
		main_window->addDockWidget(Qt::TopDockWidgetArea, dock);

		// Insert a QMainWindow into the dock widget to allow for a tool bar
		QMainWindow *dock_main = new QMainWindow(dock);
		dock_main->setWindowFlags(Qt::Widget);  // Remove Qt::Window flag

		shared_ptr<views::TraceView::View> v =
			make_shared<views::TraceView::View>(session, dock_main);
		view_docks_[dock] = v;
		session.register_view(v);

		dock_main->setCentralWidget(v.get());
		dock->setWidget(dock_main);

		dock->setFeatures(QDockWidget::DockWidgetMovable |
			QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);

		QAbstractButton *close_btn =
			dock->findChildren<QAbstractButton*>
				("qt_dockwidget_closebutton").front();

		connect(close_btn, SIGNAL(clicked(bool)),
			this, SLOT(on_view_close_clicked()));

		if (type == views::ViewTypeTrace) {
			connect(&session, SIGNAL(trigger_event(util::Timestamp)),
				qobject_cast<views::ViewBase*>(v.get()),
				SLOT(trigger_event(util::Timestamp)));

			v->enable_sticky_scrolling(action_view_sticky_scrolling_->isChecked());
			v->enable_coloured_bg(action_view_coloured_bg_->isChecked());

			shared_ptr<MainBar> main_bar = session.main_bar();
			if (!main_bar) {
				main_bar = make_shared<MainBar>(session, *this);
				dock_main->addToolBar(main_bar.get());
				session.set_main_bar(main_bar);

				connect(main_bar.get(), SIGNAL(new_view(Session*)),
					this, SLOT(on_new_view(Session*)));
			}
			main_bar->action_view_show_cursors()->setChecked(v->cursors_shown());

			connect(v.get(), SIGNAL(always_zoom_to_fit_changed(bool)),
				main_bar.get(), SLOT(on_always_zoom_to_fit_changed(bool)));
		}

		return v;
	}

	return nullptr;
}

shared_ptr<Session> MainWindow::add_session()
{
	static int last_session_id = 1;
	QString name = tr("Untitled-%1").arg(last_session_id++);

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
	int h = new_session_button_->height();

	for (shared_ptr<views::ViewBase> view : session->views()) {
		// Find the dock the view is contained in and remove it
		for (auto entry : view_docks_)
			if (entry.second == view) {
				// Remove the view from the session
				session->deregister_view(view);

				// Remove the view from its parent; otherwise, Qt will
				// call deleteLater() on it, which causes a double free
				// since the shared_ptr in view_docks_ doesn't know
				// that Qt keeps a pointer to the view around
				entry.second->setParent(0);

				// Remove this entry from the container
				view_docks_.erase(entry.first);
			}
	}

	QMainWindow *window = session_windows_.at(session);
	session_selector_.removeTab(session_selector_.indexOf(window));

	session_windows_.erase(session);

	if (last_focused_session_ == session)
		last_focused_session_.reset();

	sessions_.remove_if([&](shared_ptr<Session> s) {
		return s == session; });

	if (sessions_.empty()) {
		// When there are no more tabs, the height of the QTabWidget
		// drops to zero. We must prevent this to keep the static
		// widgets visible
		for (QWidget *w : static_tab_widget_->findChildren<QWidget*>())
			w->setMinimumHeight(h);

		int margin = static_tab_widget_->layout()->contentsMargins().bottom();
		static_tab_widget_->setMinimumHeight(h + 2 * margin);
		session_selector_.setMinimumHeight(h + 2 * margin);

		// Update the window title if there is no view left to
		// generate focus change events
		setWindowTitle(WindowTitle);
	}
}

void MainWindow::setup_ui()
{
	setObjectName(QString::fromUtf8("MainWindow"));

	setCentralWidget(&session_selector_);

	// Set the window icon
	QIcon icon;
	icon.addFile(QString(":/icons/sigrok-logo-notext.png"));
	setWindowIcon(icon);

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

	action_about_->setObjectName(QString::fromUtf8("actionAbout"));
	action_about_->setText(tr("&About..."));

	// Set up the tab area
	new_session_button_ = new QToolButton();
	new_session_button_->setIcon(QIcon::fromTheme("document-new",
		QIcon(":/icons/document-new.png")));
	new_session_button_->setAutoRaise(true);

	run_stop_button_ = new QToolButton();
	run_stop_button_->setAutoRaise(true);
	run_stop_button_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	run_stop_button_->setShortcut(QKeySequence(Qt::Key_Space));

	QHBoxLayout* layout = new QHBoxLayout();
	layout->setContentsMargins(2, 2, 2, 2);
	layout->addWidget(new_session_button_);
	layout->addWidget(run_stop_button_);

	static_tab_widget_ = new QWidget();
	static_tab_widget_->setLayout(layout);

	session_selector_.setCornerWidget(static_tab_widget_, Qt::TopLeftCorner);
	session_selector_.setTabsClosable(true);

	connect(new_session_button_, SIGNAL(clicked(bool)),
		this, SLOT(on_new_session_clicked()));
	connect(run_stop_button_, SIGNAL(clicked(bool)),
		this, SLOT(on_run_stop_clicked()));
	connect(&session_state_mapper_, SIGNAL(mapped(QObject*)),
		this, SLOT(on_capture_state_changed(QObject*)));

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
	int id = 0;

	settings.beginGroup("MainWindow");
	settings.setValue("state", saveState());
	settings.setValue("geometry", saveGeometry());
	settings.endGroup();

	for (shared_ptr<Session> session : sessions_) {
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

void MainWindow::restore_ui_settings()
{
	QSettings settings;
	int i, session_count;

	settings.beginGroup("MainWindow");

	if (settings.contains("geometry")) {
		restoreGeometry(settings.value("geometry").toByteArray());
		restoreState(settings.value("state").toByteArray());
	} else
		resize(1000, 720);

	settings.endGroup();

	session_count = settings.value("sessions", 0).toInt();

	for (i = 0; i < session_count; i++) {
		settings.beginGroup("Session" + QString::number(i));
		shared_ptr<Session> session = add_session();
		session->restore_settings(settings);
		settings.endGroup();
	}
}

std::shared_ptr<Session> MainWindow::get_tab_session(int index) const
{
	// Find the session that belongs to the tab's main window
	for (auto entry : session_windows_)
		if (entry.second == session_selector_.widget(index))
			return entry.first;

	return nullptr;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
	save_ui_settings();
	event->accept();
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

void MainWindow::session_error(const QString text, const QString info_text)
{
	QMetaObject::invokeMethod(this, "show_session_error",
		Qt::QueuedConnection, Q_ARG(QString, text),
		Q_ARG(QString, info_text));
}

void MainWindow::show_session_error(const QString text, const QString info_text)
{
	QMessageBox msg(this);
	msg.setText(text);
	msg.setInformativeText(info_text);
	msg.setStandardButtons(QMessageBox::Ok);
	msg.setIcon(QMessageBox::Warning);
	msg.exec();
}

void MainWindow::on_add_view(const QString &title, views::ViewType type,
	Session *session)
{
	// We get a pointer and need a reference
	for (std::shared_ptr<Session> s : sessions_)
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
			session_error("Capture failed", message); });
		break;
	case Session::AwaitingTrigger:
	case Session::Running:
		session->stop_capture();
		break;
	}
}

void MainWindow::on_session_name_changed()
{
	// Update the corresponding dock widget's name(s)
	Session *session = qobject_cast<Session*>(QObject::sender());
	assert(session);

	for (shared_ptr<views::ViewBase> view : session->views()) {
		// Get the dock that contains the view
		for (auto entry : view_docks_)
			if (entry.second == view) {
				entry.first->setObjectName(session->name());
				entry.first->setWindowTitle(session->name());
			}
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
	for (std::shared_ptr<Session> s : sessions_)
		if (s.get() == session)
			add_view(session->name(), views::ViewTypeTrace, *s);
}

void MainWindow::on_view_close_clicked()
{
	// Find the dock widget that contains the close button that was clicked
	QObject *w = QObject::sender();
	QDockWidget *dock = 0;

	while (w) {
	    dock = qobject_cast<QDockWidget*>(w);
	    if (dock)
	        break;
	    w = w->parent();
	}

	// Get the view contained in the dock widget
	shared_ptr<views::ViewBase> view;

	for (auto entry : view_docks_)
		if (entry.first == dock)
			view = entry.second;

	// Deregister the view
	for (shared_ptr<Session> session : sessions_) {
		if (!session->has_view(view))
			continue;

		// Also destroy the entire session if its main view is closing
		if (view == session->main_view()) {
			remove_session(session);
			break;
		} else
			session->deregister_view(view);
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
	// TODO Ask user if this is intended in case data is unsaved

	shared_ptr<Session> session = get_tab_session(index);

	if (session)
		remove_session(session);
}

void MainWindow::on_actionViewStickyScrolling_triggered()
{
	shared_ptr<views::ViewBase> viewbase = get_active_view();
	views::TraceView::View* view =
		qobject_cast<views::TraceView::View*>(viewbase.get());
	if (view)
		view->enable_sticky_scrolling(action_view_sticky_scrolling_->isChecked());
}

void MainWindow::on_actionViewColouredBg_triggered()
{
	shared_ptr<views::ViewBase> viewbase = get_active_view();
	views::TraceView::View* view =
			qobject_cast<views::TraceView::View*>(viewbase.get());
	if (view)
		view->enable_coloured_bg(action_view_coloured_bg_->isChecked());
}

void MainWindow::on_actionAbout_triggered()
{
	dialogs::About dlg(device_manager_.context(), this);
	dlg.exec();
}

} // namespace pv
