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

#ifndef PULSEVIEW_GLOBALSETTINGS_HPP
#define PULSEVIEW_GLOBALSETTINGS_HPP

#include <functional>
#include <map>

#include <QSettings>
#include <QString>
#include <QVariant>

namespace pv {

class GlobalSettings : public QSettings
{
	Q_OBJECT

public:
	static const QString Key_View_AlwaysZoomToFit;
	static const QString Key_View_ColouredBG;

public:
	GlobalSettings();

	static void register_change_handler(const QString key,
		std::function<void(QVariant)> cb);

	void setValue(const QString& key, const QVariant& value);

private:
	static std::multimap< QString, std::function<void(QVariant)> > callbacks_;
};

} // namespace pv

#endif // PULSEVIEW_GLOBALSETTINGS_HPP
