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

#ifndef PULSEVIEW_PV_SETTINGS_HPP
#define PULSEVIEW_PV_SETTINGS_HPP

#include <QCheckBox>
#include <QDialog>
#include <QListWidget>
#include <QStackedWidget>

namespace pv {

class DeviceManager;

namespace dialogs {

class Settings : public QDialog
{
	Q_OBJECT

public:
	Settings(DeviceManager &device_manager, QWidget *parent = nullptr);

	void create_pages();
	QCheckBox *create_checkbox(const QString& key, const char* slot) const;

	QWidget *get_view_settings_form(QWidget *parent) const;
	QWidget *get_decoder_settings_form(QWidget *parent) const;
	QWidget *get_about_page(QWidget *parent) const;

	void accept();
	void reject();

private Q_SLOTS:
	void on_page_changed(QListWidgetItem *current, QListWidgetItem *previous);
	void on_view_alwaysZoomToFit_changed(int state);
	void on_view_colouredBG_changed(int state);
	void on_view_stickyScrolling_changed(int state);
	void on_view_showSamplingPoints_changed(int state);
	void on_view_showAnalogMinorGrid_changed(int state);
	void on_dec_initialStateConfigurable_changed(int state);

private:
	DeviceManager &device_manager_;
	QListWidget *page_list;
	QStackedWidget *pages;
};

} // namespace dialogs
} // namespace pv

#endif // PULSEVIEW_PV_SETTINGS_HPP
