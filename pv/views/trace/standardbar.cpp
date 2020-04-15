/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2016 Soeren Apel <soeren@apelpie.net>
 * Copyright (C) 2012-2015 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#include <QAction>
#include <QMessageBox>

#include "standardbar.hpp"
#include "view.hpp"

#include <pv/mainwindow.hpp>

using pv::views::trace::View;

namespace pv {
namespace views {

namespace trace {

StandardBar::StandardBar(Session &session, QWidget *parent,
	View *view, bool add_default_widgets) :
	QToolBar("Standard Trace View Toolbar", parent),
	session_(session),
	view_(view),
	action_view_zoom_in_(new QAction(this)),
	action_view_zoom_out_(new QAction(this)),
	action_view_zoom_fit_(new QAction(this)),
	action_view_trigger_scrolling_(new QAction(this)),
	action_view_show_cursors_(new QAction(this)),
	segment_display_mode_selector_(new QToolButton(this)),
	action_sdm_last_(new QAction(this)),
	action_sdm_last_complete_(new QAction(this)),
	action_sdm_single_(new QAction(this)),
	segment_selector_(new QSpinBox(this))
{
	setObjectName(QString::fromUtf8("StandardBar"));

	// Actions
	action_view_zoom_in_->setText(tr("Zoom &In"));
	action_view_zoom_in_->setIcon(QIcon::fromTheme("zoom-in",
		QIcon(":/icons/zoom-in.png")));
	// simply using Qt::Key_Plus shows no + in the menu
	action_view_zoom_in_->setShortcut(QKeySequence::ZoomIn);
	connect(action_view_zoom_in_, SIGNAL(triggered(bool)),
		this, SLOT(on_actionViewZoomIn_triggered()));

	action_view_zoom_out_->setText(tr("Zoom &Out"));
	action_view_zoom_out_->setIcon(QIcon::fromTheme("zoom-out",
		QIcon(":/icons/zoom-out.png")));
	action_view_zoom_out_->setShortcut(QKeySequence::ZoomOut);
	connect(action_view_zoom_out_, SIGNAL(triggered(bool)),
		this, SLOT(on_actionViewZoomOut_triggered()));

	action_view_zoom_fit_->setCheckable(true);
	action_view_zoom_fit_->setText(tr("Zoom to &Fit"));
	action_view_zoom_fit_->setIcon(QIcon::fromTheme("zoom-fit-best",
		QIcon(":/icons/zoom-fit-best.png")));
	action_view_zoom_fit_->setShortcut(QKeySequence(Qt::Key_F));
	connect(action_view_zoom_fit_, SIGNAL(triggered(bool)),
		this, SLOT(on_actionViewZoomFit_triggered(bool)));

	action_view_trigger_scrolling_->setCheckable(true);
	action_view_trigger_scrolling_->setText(tr("Scroll to &Trigger"));
	action_view_trigger_scrolling_->setIcon(QIcon::fromTheme("trigger-scrolling",
		QIcon(":/icons/trigger-scrolling.svg")));
	action_view_trigger_scrolling_->setShortcut(QKeySequence(Qt::Key_T));
	connect(action_view_trigger_scrolling_, SIGNAL(triggered(bool)),
		this, SLOT(on_actionViewScrollToTrigger_triggered(bool)));

	action_view_show_cursors_->setCheckable(true);
	action_view_show_cursors_->setIcon(QIcon(":/icons/show-cursors.svg"));
	action_view_show_cursors_->setShortcut(QKeySequence(Qt::Key_C));
	connect(action_view_show_cursors_, SIGNAL(triggered(bool)),
		this, SLOT(on_actionViewShowCursors_triggered()));
	action_view_show_cursors_->setText(tr("Show &Cursors"));

	action_sdm_last_->setIcon(QIcon(":/icons/view-displaymode-last_segment.svg"));
	action_sdm_last_->setText(tr("Display last segment only"));
	connect(action_sdm_last_, SIGNAL(triggered(bool)),
		this, SLOT(on_actionSDMLast_triggered()));

	action_sdm_last_complete_->setIcon(QIcon(":/icons/view-displaymode-last_complete_segment.svg"));
	action_sdm_last_complete_->setText(tr("Display last complete segment only"));
	connect(action_sdm_last_complete_, SIGNAL(triggered(bool)),
		this, SLOT(on_actionSDMLastComplete_triggered()));

	action_sdm_single_->setIcon(QIcon(":/icons/view-displaymode-single_segment.svg"));
	action_sdm_single_->setText(tr("Display a single segment"));
	connect(action_view_show_cursors_, SIGNAL(triggered(bool)),
		this, SLOT(on_actionSDMSingle_triggered()));

	segment_display_mode_selector_->addAction(action_sdm_last_);
	segment_display_mode_selector_->addAction(action_sdm_last_complete_);
	segment_display_mode_selector_->addAction(action_sdm_single_);
	segment_display_mode_selector_->setPopupMode(QToolButton::InstantPopup);
	segment_display_mode_selector_->hide();

	segment_selector_->setMinimum(1);
	segment_selector_->hide();

	connect(&session_, SIGNAL(new_segment(int)),
		this, SLOT(on_new_segment(int)));

	connect(&session_, SIGNAL(segment_completed(int)),
		view_, SLOT(on_segment_completed(int)));

	connect(segment_selector_, SIGNAL(valueChanged(int)),
		this, SLOT(on_segment_selected(int)));
	connect(view_, SIGNAL(segment_changed(int)),
		this, SLOT(on_segment_changed(int)));

	connect(this, SIGNAL(segment_selected(int)),
		view_, SLOT(on_segment_changed(int)));

	connect(view_, SIGNAL(segment_display_mode_changed(int, bool)),
		this, SLOT(on_segment_display_mode_changed(int, bool)));

	connect(view_, SIGNAL(always_zoom_to_fit_changed(bool)),
		this, SLOT(on_always_zoom_to_fit_changed(bool)));

	connect(view_, SIGNAL(trigger_scrolling_changed(bool)),
		this, SLOT(on_trigger_scrolling_changed(bool)));

	connect(view_, SIGNAL(cursor_state_changed(bool)),
		this, SLOT(on_cursor_state_changed(bool)));

	if (add_default_widgets)
		add_toolbar_widgets();
}

Session &StandardBar::session() const
{
	return session_;
}

void StandardBar::add_toolbar_widgets()
{
	// Setup the toolbar
	addAction(action_view_zoom_in_);
	addAction(action_view_zoom_out_);
	addAction(action_view_zoom_fit_);
	addAction(action_view_trigger_scrolling_);
	addSeparator();
	addAction(action_view_show_cursors_);
	multi_segment_actions_.push_back(addSeparator());
	multi_segment_actions_.push_back(addWidget(segment_display_mode_selector_));
	multi_segment_actions_.push_back(addWidget(segment_selector_));
	addSeparator();

	// Hide the multi-segment UI until we know that there are multiple segments
	show_multi_segment_ui(false);
}

void StandardBar::show_multi_segment_ui(const bool state)
{
	for (QAction* action : multi_segment_actions_)
		action->setVisible(state);

	on_segment_display_mode_changed(view_->segment_display_mode(),
		view_->segment_is_selectable());
}

QAction* StandardBar::action_view_zoom_in() const
{
	return action_view_zoom_in_;
}

QAction* StandardBar::action_view_zoom_out() const
{
	return action_view_zoom_out_;
}

QAction* StandardBar::action_view_zoom_fit() const
{
	return action_view_zoom_fit_;
}

QAction* StandardBar::action_view_show_cursors() const
{
	return action_view_show_cursors_;
}

void StandardBar::on_actionViewZoomIn_triggered()
{
	view_->zoom(1);
}

void StandardBar::on_actionViewZoomOut_triggered()
{
	view_->zoom(-1);
}

void StandardBar::on_actionViewZoomFit_triggered(bool checked)
{
	view_->zoom_fit(checked);
}

void StandardBar::on_actionViewScrollToTrigger_triggered(bool checked)
{
	view_->trigger_scrolling(checked);
}

void StandardBar::on_actionViewShowCursors_triggered()
{
	const bool show = action_view_show_cursors_->isChecked();

	if (show)
		view_->center_cursors();

	view_->show_cursors(show);
}

void StandardBar::on_actionSDMLast_triggered()
{
	view_->set_segment_display_mode(Trace::ShowLastSegmentOnly);
}

void StandardBar::on_actionSDMLastComplete_triggered()
{
	view_->set_segment_display_mode(Trace::ShowLastCompleteSegmentOnly);
}

void StandardBar::on_actionSDMSingle_triggered()
{
	view_->set_segment_display_mode(Trace::ShowSingleSegmentOnly);
}

void StandardBar::on_always_zoom_to_fit_changed(bool state)
{
	action_view_zoom_fit_->setChecked(state);
}

void StandardBar::on_trigger_scrolling_changed(bool state)
{
	action_view_trigger_scrolling_->setChecked(state);
}

void StandardBar::on_new_segment(int new_segment_id)
{
	if (new_segment_id > 0) {
		show_multi_segment_ui(true);
		segment_selector_->setMaximum(new_segment_id + 1);
	} else
		show_multi_segment_ui(false);
}

void StandardBar::on_segment_changed(int segment_id)
{
	// We need to adjust the value by 1 because internally, segments
	// start at 0 while they start with 1 for the spinbox
	const uint32_t ui_segment_id = segment_id + 1;

	// This is called when the current segment was changed
	// by other parts of the UI, e.g. the view itself

	// Make sure our value isn't limited by a too low maximum
	// Note: this can happen if on_segment_changed() is called before
	// on_new_segment()
	if ((uint32_t)segment_selector_->maximum() < ui_segment_id)
		segment_selector_->setMaximum(ui_segment_id);

	segment_selector_->setValue(ui_segment_id);
}

void StandardBar::on_segment_selected(int ui_segment_id)
{
	// We need to adjust the value by 1 because internally, segments
	// start at 0 while they start with 1 for the spinbox
	const uint32_t segment_id = ui_segment_id - 1;

	// This is called when the user selected a segment using the spin box
	// or when the value of the spinbox was assigned a new value. Since we
	// only care about the former, we filter out the latter:
	if (segment_id == view_->current_segment())
		return;

	// No matter which segment display mode we were in, we now show a single segment
	if (view_->segment_display_mode() != Trace::ShowSingleSegmentOnly)
		on_actionSDMSingle_triggered();

	segment_selected(segment_id);
}

void StandardBar::on_segment_display_mode_changed(int mode, bool segment_selectable)
{
	segment_selector_->setReadOnly(!segment_selectable);

	switch ((Trace::SegmentDisplayMode)mode) {
	case Trace::ShowLastSegmentOnly:
		segment_display_mode_selector_->setDefaultAction(action_sdm_last_);
		break;
	case Trace::ShowLastCompleteSegmentOnly:
		segment_display_mode_selector_->setDefaultAction(action_sdm_last_complete_);
		break;
	case Trace::ShowSingleSegmentOnly:
		segment_display_mode_selector_->setDefaultAction(action_sdm_single_);
		break;
	default:
		break;
	}
}

void StandardBar::on_cursor_state_changed(bool show)
{
	action_view_show_cursors_->setChecked(show);
}

} // namespace trace
} // namespace views
} // namespace pv
