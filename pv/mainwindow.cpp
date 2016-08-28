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

MainWindow::MainWindow(DeviceManager &device_manager,
	string open_file_name, string open_file_format,
	QWidget *parent) :
	QMainWindow(parent),
	device_manager_(device_manager),
	action_view_sticky_scrolling_(new QAction(this)),
	action_view_coloured_bg_(new QAction(this)),
	action_about_(new QAction(this))
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
	for (auto entry : view_docks_) {

		const std::shared_ptr<QDockWidget> dock = entry.first;

		// Remove view from the dock widget's QMainWindow
		QMainWindow *dock_main = dynamic_cast<QMainWindow*>(dock->widget());
		dock_main->setCentralWidget(0);

		// Remove the QMainWindow
		dock->setWidget(0);

		const std::shared_ptr<pv::view::View> view = entry.second;

		for (shared_ptr<Session> session : sessions_)
			if (session->has_view(view))
				session->deregister_view(view);
	}
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

	if (type == pv::view::TraceView) {
		shared_ptr<QDockWidget> dock = make_shared<QDockWidget>(title, this);
		dock->setObjectName(title);
		addDockWidget(Qt::TopDockWidgetArea, dock.get());

		// Insert a QMainWindow into the dock widget to allow for a tool bar
		QMainWindow *dock_main = new QMainWindow(dock.get());
		dock_main->setWindowFlags(Qt::Widget);  // Remove Qt::Window flag

		v = make_shared<pv::view::View>(session, dock_main);
		view_docks_[dock] = v;
		session.register_view(v);

		dock_main->setCentralWidget(v.get());
		dock->setWidget(dock_main);

		dock->setFeatures(QDockWidget::DockWidgetMovable |
			QDockWidget::DockWidgetFloatable);

		if (type == view::TraceView) {
			connect(&session, SIGNAL(trigger_event(util::Timestamp)), v.get(),
				SLOT(trigger_event(util::Timestamp)));

			v->enable_sticky_scrolling(action_view_sticky_scrolling_->isChecked());
			v->enable_coloured_bg(action_view_coloured_bg_->isChecked());

			shared_ptr<MainBar> main_bar = session.main_bar();
			if (!main_bar) {
				main_bar = make_shared<MainBar>(session, *this);
				dock_main->addToolBar(main_bar.get());
				session.set_main_bar(main_bar);
			}
			main_bar->action_view_show_cursors()->setChecked(v->cursors_shown());

			connect(v.get(), SIGNAL(always_zoom_to_fit_changed(bool)),
				main_bar.get(), SLOT(on_always_zoom_to_fit_changed(bool)));
		}
	}

	return v;
}

shared_ptr<Session> MainWindow::add_session()
{
	int id = sessions_.size();
	QString name = tr("Untitled-%1").arg(id + 1);

	shared_ptr<Session> session = make_shared<Session>(device_manager_, name);

	sessions_.push_back(session);

	shared_ptr<view::View> main_view =
		add_view(name, pv::view::TraceView, *session);

	return session;
}

void MainWindow::setup_ui()
{
	setObjectName(QString::fromUtf8("MainWindow"));

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

	// Set the title
	setWindowTitle(tr("PulseView"));
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
		// Ignore sessions using the demo device
		if (session->device()) {
			shared_ptr<devices::HardwareDevice> device =
				dynamic_pointer_cast< devices::HardwareDevice >
				(session->device());

			if (device->hardware_device()->driver()->name() == "demo")
				continue;
		}

		settings.beginGroup("Session" + QString::number(id++));
		settings.remove("");  // Remove all keys in this group
		session->save_settings(settings);
		settings.endGroup();
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

void MainWindow::on_actionAbout_triggered()
{
	dialogs::About dlg(device_manager_.context(), this);
	dlg.exec();
}

} // namespace pv
