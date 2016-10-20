/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2016 Soeren Apel <soeren@apelpie.net>
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

#ifndef PULSEVIEW_PV_VIEWS_TRACE_STANDARDBAR_HPP
#define PULSEVIEW_PV_VIEWS_TRACE_STANDARDBAR_HPP

#include <stdint.h>

#include <QAction>
#include <QToolBar>
#include <QWidget>

#include <pv/session.hpp>

namespace pv {

class MainWindow;
class Session;

namespace views {

namespace TraceView {
class View;
}

namespace trace {

class StandardBar : public QToolBar
{
	Q_OBJECT

public:
	StandardBar(Session &session, QWidget *parent,
		TraceView::View *view, bool add_default_widgets=true);

	Session &session(void) const;

	QAction* action_view_zoom_in() const;
	QAction* action_view_zoom_out() const;
	QAction* action_view_zoom_fit() const;
	QAction* action_view_zoom_one_to_one() const;
	QAction* action_view_show_cursors() const;

protected:
	virtual void add_toolbar_widgets();

	Session &session_;
	TraceView::View *view_;

	QAction *const action_view_zoom_in_;
	QAction *const action_view_zoom_out_;
	QAction *const action_view_zoom_fit_;
	QAction *const action_view_zoom_one_to_one_;
	QAction *const action_view_show_cursors_;

protected Q_SLOTS:
	void on_actionViewZoomIn_triggered();

	void on_actionViewZoomOut_triggered();

	void on_actionViewZoomFit_triggered(bool checked);

	void on_actionViewZoomOneToOne_triggered();

	void on_actionViewShowCursors_triggered();

	void on_always_zoom_to_fit_changed(bool state);
};

} // namespace trace
} // namespace views
} // namespace pv

#endif // PULSEVIEW_PV_VIEWS_TRACE_STANDARDBAR_HPP
