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

#include <QDialog>

namespace pv {
namespace dialogs {

class Settings : public QDialog
{
	Q_OBJECT

public:
	Settings(QWidget *parent = 0);

	QWidget *get_view_settings_form(QWidget *parent) const;

	void accept();
	void reject();

private Q_SLOTS:
	void on_view_alwaysZoomToFit_changed(int state);
	void on_view_colouredBG_changed(int state);
};

} // namespace dialogs
} // namespace pv

#endif // PULSEVIEW_PV_SETTINGS_HPP
