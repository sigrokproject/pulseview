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
	action_view_zoom_one_to_one_(new QAction(this)),
	action_view_show_cursors_(new QAction(this)),
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

	action_view_zoom_one_to_one_->setText(tr("Zoom to O&ne-to-One"));
	action_view_zoom_one_to_one_->setIcon(QIcon::fromTheme("zoom-original",
		QIcon(":/icons/zoom-original.png")));
	action_view_zoom_one_to_one_->setShortcut(QKeySequence(Qt::Key_O));
	connect(action_view_zoom_one_to_one_, SIGNAL(triggered(bool)),
		this, SLOT(on_actionViewZoomOneToOne_triggered()));

	action_view_show_cursors_->setCheckable(true);
	action_view_show_cursors_->setIcon(QIcon(":/icons/show-cursors.svg"));
	action_view_show_cursors_->setShortcut(QKeySequence(Qt::Key_C));
	connect(action_view_show_cursors_, SIGNAL(triggered(bool)),
		this, SLOT(on_actionViewShowCursors_triggered()));
	action_view_show_cursors_->setText(tr("Show &Cursors"));

	segment_selector_->setMinimum(1);
	segment_selector_->hide();
	connect(&session_, SIGNAL(new_segment(int)),
		this, SLOT(on_new_segment(int)));
	connect(segment_selector_, SIGNAL(valueChanged(int)),
		view_, SLOT(on_segment_changed(int)));
	connect(view_, SIGNAL(segment_changed(int)),
		this, SLOT(on_segment_changed(int)));
	connect(view_, SIGNAL(segment_display_mode_changed(bool)),
		this, SLOT(on_segment_display_mode_changed(bool)));

	connect(view_, SIGNAL(always_zoom_to_fit_changed(bool)),
		this, SLOT(on_always_zoom_to_fit_changed(bool)));

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
	addAction(action_view_zoom_one_to_one_);
	addSeparator();
	addAction(action_view_show_cursors_);
	multi_segment_actions_.push_back(addSeparator());
	multi_segment_actions_.push_back(addWidget(segment_selector_));
	addSeparator();

	// Hide the multi-segment UI until we know that there are multiple segments
	show_multi_segment_ui(false);
}

void StandardBar::show_multi_segment_ui(const bool state)
{
	for (QAction* action : multi_segment_actions_)
		action->setVisible(state);

	on_segment_display_mode_changed(view_->segment_is_selectable());
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

QAction* StandardBar::action_view_zoom_one_to_one() const
{
	return action_view_zoom_one_to_one_;
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

void StandardBar::on_actionViewZoomOneToOne_triggered()
{
	view_->zoom_one_to_one();
}

void StandardBar::on_actionViewShowCursors_triggered()
{
	const bool show = !view_->cursors_shown();
	if (show)
		view_->centre_cursors();

	view_->show_cursors(show);
}

void StandardBar::on_always_zoom_to_fit_changed(bool state)
{
	action_view_zoom_fit_->setChecked(state);
}

void StandardBar::on_new_segment(int new_segment_id)
{
	if (new_segment_id > 1) {
		show_multi_segment_ui(true);
		segment_selector_->setMaximum(new_segment_id);
	} else
		show_multi_segment_ui(false);
}

void StandardBar::on_segment_changed(int segment_id)
{
	// This is called when the current segment was changed
	// by other parts of the UI, e.g. the view itself
	segment_selector_->setValue(segment_id);
}

void StandardBar::on_segment_display_mode_changed(bool segment_selectable)
{
	segment_selector_->setReadOnly(!segment_selectable);
}

} // namespace trace
} // namespace views
} // namespace pv
