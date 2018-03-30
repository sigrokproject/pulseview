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

#include <QApplication>
#include <QDebug>
#include <QFontMetrics>
#include <QString>

using std::map;
using std::vector;

namespace pv {

const QString GlobalSettings::Key_View_ZoomToFitDuringAcq = "View_ZoomToFitDuringAcq";
const QString GlobalSettings::Key_View_ZoomToFitAfterAcq = "View_ZoomToFitAfterAcq";
const QString GlobalSettings::Key_View_TriggerIsZeroTime = "View_TriggerIsZeroTime";
const QString GlobalSettings::Key_View_ColouredBG = "View_ColouredBG";
const QString GlobalSettings::Key_View_StickyScrolling = "View_StickyScrolling";
const QString GlobalSettings::Key_View_ShowSamplingPoints = "View_ShowSamplingPoints";
const QString GlobalSettings::Key_View_ShowAnalogMinorGrid = "View_ShowAnalogMinorGrid";
const QString GlobalSettings::Key_View_ConversionThresholdDispMode = "View_ConversionThresholdDispMode";
const QString GlobalSettings::Key_View_DefaultDivHeight = "View_DefaultDivHeight";
const QString GlobalSettings::Key_View_DefaultLogicHeight = "View_DefaultLogicHeight";
const QString GlobalSettings::Key_Dec_InitialStateConfigurable = "Dec_InitialStateConfigurable";
const QString GlobalSettings::Key_Log_BufferSize = "Log_BufferSize";
const QString GlobalSettings::Key_Log_NotifyOfStacktrace = "Log_NotifyOfStacktrace";

vector<GlobalSettingsInterface*> GlobalSettings::callbacks_;
bool GlobalSettings::tracking_ = false;
map<QString, QVariant> GlobalSettings::tracked_changes_;

GlobalSettings::GlobalSettings() :
	QSettings()
{
	beginGroup("Settings");
}

void GlobalSettings::set_defaults_where_needed()
{
	// Enable zoom-to-fit after acquisition by default
	if (!contains(Key_View_ZoomToFitAfterAcq))
		setValue(Key_View_ZoomToFitAfterAcq, true);

	// Enable coloured trace backgrounds by default
	if (!contains(Key_View_ColouredBG))
		setValue(Key_View_ColouredBG, true);

	// Enable showing sampling points by default
	if (!contains(Key_View_ShowSamplingPoints))
		setValue(Key_View_ShowSamplingPoints, true);

	if (!contains(Key_View_DefaultDivHeight))
		setValue(Key_View_DefaultDivHeight,
		3 * QFontMetrics(QApplication::font()).height());

	if (!contains(Key_View_DefaultLogicHeight))
		setValue(Key_View_DefaultLogicHeight,
		2 * QFontMetrics(QApplication::font()).height());

	// Default to 500 lines of backlog
	if (!contains(Key_Log_BufferSize))
		setValue(Key_Log_BufferSize, 500);

	// Notify user of existing stack trace by default
	if (!contains(Key_Log_NotifyOfStacktrace))
		setValue(Key_Log_NotifyOfStacktrace, true);
}

void GlobalSettings::add_change_handler(GlobalSettingsInterface *cb)
{
	callbacks_.push_back(cb);
}

void GlobalSettings::remove_change_handler(GlobalSettingsInterface *cb)
{
	for (auto cb_it = callbacks_.begin(); cb_it != callbacks_.end(); cb_it++)
		if (*cb_it == cb) {
			callbacks_.erase(cb_it);
			break;
		}
}

void GlobalSettings::setValue(const QString &key, const QVariant &value)
{
	// Save previous value if we're tracking changes,
	// not altering an already-existing saved setting
	if (tracking_)
		tracked_changes_.emplace(key, QSettings::value(key));

	QSettings::setValue(key, value);

	qDebug() << "Setting" << key << "changed to" << value;

	// Call all registered callbacks
	for (GlobalSettingsInterface *cb : callbacks_)
		cb->on_setting_changed(key, value);
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

void GlobalSettings::store_gvariant(QSettings &settings, GVariant *v)
{
	const GVariantType *var_type = g_variant_get_type(v);
	char *var_type_str = g_variant_type_dup_string(var_type);

	QByteArray var_data = QByteArray((const char*)g_variant_get_data(v),
		g_variant_get_size(v));

	settings.setValue("value", var_data);
	settings.setValue("type", var_type_str);

	g_free(var_type_str);
}

GVariant* GlobalSettings::restore_gvariant(QSettings &settings)
{
	QString raw_type = settings.value("type").toString();
	GVariantType *var_type = g_variant_type_new(raw_type.toUtf8());

	QByteArray data = settings.value("value").toByteArray();

	gpointer var_data = g_memdup((gconstpointer)data.constData(),
		(guint)data.size());

	GVariant *value = g_variant_new_from_data(var_type, var_data,
		data.size(), false, g_free, var_data);

	g_variant_type_free(var_type);

	return value;
}


} // namespace pv
