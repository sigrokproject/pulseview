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

#include <cstdint>

#include <QAction>
#include <QSpinBox>
#include <QToolBar>
#include <QToolButton>
#include <QWidget>

#include <pv/session.hpp>

#include "trace.hpp"

namespace pv {

class MainWindow;
class Session;

namespace views {

namespace trace {
class View;
}

namespace trace {

class StandardBar : public QToolBar
{
	Q_OBJECT

public:
	StandardBar(Session &session, QWidget *parent,
		trace::View *view, bool add_default_widgets = true);

	Session &session() const;

	QAction* action_view_zoom_in() const;
	QAction* action_view_zoom_out() const;
	QAction* action_view_zoom_fit() const;
	QAction* action_view_show_cursors() const;

protected:
	virtual void add_toolbar_widgets();

	virtual void show_multi_segment_ui(const bool state);

	Session &session_;
	trace::View *view_;

	QAction *const action_view_zoom_in_;
	QAction *const action_view_zoom_out_;
	QAction *const action_view_zoom_fit_;
	QAction *const action_view_trigger_scrolling_;
	QAction *const action_view_show_cursors_;

	QToolButton *segment_display_mode_selector_;
	QAction *const action_sdm_last_;
	QAction *const action_sdm_last_complete_;
	QAction *const action_sdm_single_;

	QSpinBox *segment_selector_;

Q_SIGNALS:
	void segment_selected(int segment_id);

protected Q_SLOTS:
	void on_actionViewZoomIn_triggered();

	void on_actionViewZoomOut_triggered();

	void on_actionViewZoomFit_triggered(bool checked);
	void on_actionViewScrollToTrigger_triggered(bool checked);

	void on_actionViewShowCursors_triggered();
	void on_cursor_state_changed(bool show);

	void on_actionSDMLast_triggered();
	void on_actionSDMLastComplete_triggered();
	void on_actionSDMSingle_triggered();

	void on_always_zoom_to_fit_changed(bool state);
	void on_trigger_scrolling_changed(bool state);

	void on_new_segment(int new_segment_id);
	void on_segment_changed(int segment_id);
	void on_segment_selected(int ui_segment_id);
	void on_segment_display_mode_changed(int mode, bool segment_selectable);

private:
	vector<QAction*> multi_segment_actions_;
};

} // namespace trace
} // namespace views
} // namespace pv

#endif // PULSEVIEW_PV_VIEWS_TRACE_STANDARDBAR_HPP
