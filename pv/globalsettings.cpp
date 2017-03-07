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

#include "globalsettings.hpp"

namespace pv {

const QString GlobalSettings::Key_View_AlwaysZoomToFit = "View_AlwaysZoomToFit";
const QString GlobalSettings::Key_View_ColouredBG = "View_ColouredBG";

std::multimap< QString, std::function<void(QVariant)> > GlobalSettings::callbacks_;
bool GlobalSettings::tracking_ = false;
std::map<QString, QVariant> GlobalSettings::tracked_changes_;

GlobalSettings::GlobalSettings() :
	QSettings()
{
	beginGroup("Settings");
}

void GlobalSettings::register_change_handler(const QString key,
	std::function<void(QVariant)> cb)
{
	callbacks_.emplace(key, cb);
}

void GlobalSettings::setValue(const QString &key, const QVariant &value)
{
	// Save previous value if we're tracking changes,
	// not altering an already-existing saved setting
	if (tracking_)
		tracked_changes_.emplace(key, QSettings::value(key));

	QSettings::setValue(key, value);

	// Call all registered callbacks for this key
	auto range = callbacks_.equal_range(key);

	for (auto it = range.first; it != range.second; it++)
		it->second(value);
}

void GlobalSettings::start_tracking()
{
	tracking_ = true;
	tracked_changes_.clear();
}

void GlobalSettings::stop_tracking()
{
	tracking_ = false;
	tracked_changes_.clear();
}

void GlobalSettings::undo_tracked_changes()
{
	tracking_ = false;

	for (auto entry : tracked_changes_)
		setValue(entry.first, entry.second);

	tracked_changes_.clear();
}

} // namespace pv
