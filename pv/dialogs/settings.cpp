/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2017 Soeren Apel <soeren@apelpie.net>
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

#include "settings.hpp"
#include "pv/globalsettings.hpp"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace pv {
namespace dialogs {

Settings::Settings(QWidget *parent) :
	QDialog(parent, 0)
{
	QTabWidget *tab_stack = new QTabWidget(this);
	tab_stack->addTab(get_view_settings_form(tab_stack), tr("&Views"));

	QDialogButtonBox *button_box = new QDialogButtonBox(
		QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

	QVBoxLayout* root_layout = new QVBoxLayout(this);
	root_layout->addWidget(tab_stack);
	root_layout->addWidget(button_box);

	connect(button_box, SIGNAL(accepted()), this, SLOT(accept()));
	connect(button_box, SIGNAL(rejected()), this, SLOT(reject()));

	// Start to record changes
	GlobalSettings settings;
	settings.start_tracking();
}

QWidget *Settings::get_view_settings_form(QWidget *parent) const
{
	GlobalSettings settings;

	QWidget *form = new QWidget(parent);
	QVBoxLayout *form_layout = new QVBoxLayout(form);

	// Trace view settings
	QGroupBox *trace_view_group = new QGroupBox(tr("Trace View"));
	form_layout->addWidget(trace_view_group);

	QFormLayout *trace_view_layout = new QFormLayout();
	trace_view_group->setLayout(trace_view_layout);

	QCheckBox *coloured_bg_cb = new QCheckBox();
	coloured_bg_cb->setChecked(settings.value(GlobalSettings::Key_View_ColouredBG).toBool());
	connect(coloured_bg_cb, SIGNAL(stateChanged(int)), this, SLOT(on_view_colouredBG_changed(int)));
	trace_view_layout->addRow(tr("Use &coloured trace background"), coloured_bg_cb);

	QCheckBox *always_zoom_to_fit_cb = new QCheckBox();
	always_zoom_to_fit_cb->setChecked(settings.value(GlobalSettings::Key_View_AlwaysZoomToFit).toBool());
	connect(always_zoom_to_fit_cb, SIGNAL(stateChanged(int)), this, SLOT(on_view_alwaysZoomToFit_changed(int)));
	trace_view_layout->addRow(tr("Always zoom-to-&fit during capture"), always_zoom_to_fit_cb);

	return form;
}

void Settings::accept()
{
	GlobalSettings settings;
	settings.stop_tracking();

	QDialog::accept();
}

void Settings::reject()
{
	GlobalSettings settings;
	settings.undo_tracked_changes();

	QDialog::reject();
}

void Settings::on_view_alwaysZoomToFit_changed(int state)
{
	GlobalSettings settings;
	settings.setValue(GlobalSettings::Key_View_AlwaysZoomToFit, state ? true : false);
}

void Settings::on_view_colouredBG_changed(int state)
{
	GlobalSettings settings;
	settings.setValue(GlobalSettings::Key_View_ColouredBG, state ? true : false);
}

} // namespace dialogs
} // namespace pv
