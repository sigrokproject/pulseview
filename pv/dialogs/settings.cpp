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
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace pv {
namespace dialogs {

Settings::Settings(QWidget *parent) :
	QDialog(parent, nullptr)
{
	const int icon_size = 64;

	page_list = new QListWidget;
	page_list->setViewMode(QListView::IconMode);
	page_list->setIconSize(QSize(icon_size, icon_size));
	page_list->setMovement(QListView::Static);
	page_list->setMaximumWidth(icon_size + icon_size/2);
	page_list->setSpacing(12);

	pages = new QStackedWidget;
	create_pages();

	QHBoxLayout *tab_layout = new QHBoxLayout;
	tab_layout->addWidget(page_list);
	tab_layout->addWidget(pages, Qt::AlignLeft);

	QDialogButtonBox *button_box = new QDialogButtonBox(
		QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

	QVBoxLayout* root_layout = new QVBoxLayout(this);
	root_layout->addLayout(tab_layout);
	root_layout->addWidget(button_box);

	connect(button_box, SIGNAL(accepted()), this, SLOT(accept()));
	connect(button_box, SIGNAL(rejected()), this, SLOT(reject()));
	connect(page_list, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)),
		this, SLOT(on_page_changed(QListWidgetItem*, QListWidgetItem*)));

	// Start to record changes
	GlobalSettings settings;
	settings.start_tracking();
}

void Settings::create_pages()
{
	// View page
	pages->addWidget(get_view_settings_form(pages));

	QListWidgetItem *viewButton = new QListWidgetItem(page_list);
	viewButton->setIcon(QIcon(":/icons/sigrok-logo-notext.svg"));
	viewButton->setText(tr("Views"));
	viewButton->setTextAlignment(Qt::AlignHCenter);
	viewButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
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

void Settings::on_page_changed(QListWidgetItem *current, QListWidgetItem *previous)
{
	if (!current)
		current = previous;

	pages->setCurrentIndex(page_list->row(current));
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
