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

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/serialization/serialization.hpp>

#include <QApplication>
#include <QColor>
#include <QDebug>
#include <QFile>
#include <QFontMetrics>
#include <QPixmapCache>
#include <QString>
#include <QStyle>
#include <QtGlobal>

#include "globalsettings.hpp"
#include "application.hpp"

using std::map;
using std::pair;
using std::string;
using std::stringstream;
using std::vector;

namespace pv {

const vector< pair<QString, QString> > Themes {
	{"None" , ""},
	{"QDarkStyleSheet", ":/themes/qdarkstyle/style.qss"},
	{"DarkStyle", ":/themes/darkstyle/darkstyle.qss"}
};

const QString GlobalSettings::Key_General_Language = "General_Language";
const QString GlobalSettings::Key_General_Theme = "General_Theme";
const QString GlobalSettings::Key_General_Style = "General_Style";
const QString GlobalSettings::Key_General_SaveWithSetup = "General_SaveWithSetup";
const QString GlobalSettings::Key_General_StartAllSessions = "General_StartAllSessions";
const QString GlobalSettings::Key_View_ZoomToFitDuringAcq = "View_ZoomToFitDuringAcq";
const QString GlobalSettings::Key_View_ZoomToFitAfterAcq = "View_ZoomToFitAfterAcq";
const QString GlobalSettings::Key_View_TriggerIsZeroTime = "View_TriggerIsZeroTime";
const QString GlobalSettings::Key_View_ColoredBG = "View_ColoredBG";
const QString GlobalSettings::Key_View_StickyScrolling = "View_StickyScrolling";
const QString GlobalSettings::Key_View_ShowSamplingPoints = "View_ShowSamplingPoints";
const QString GlobalSettings::Key_View_FillSignalHighAreas = "View_FillSignalHighAreas";
const QString GlobalSettings::Key_View_FillSignalHighAreaColor = "View_FillSignalHighAreaColor";
const QString GlobalSettings::Key_View_ShowAnalogMinorGrid = "View_ShowAnalogMinorGrid";
const QString GlobalSettings::Key_View_ConversionThresholdDispMode = "View_ConversionThresholdDispMode";
const QString GlobalSettings::Key_View_DefaultDivHeight = "View_DefaultDivHeight";
const QString GlobalSettings::Key_View_DefaultLogicHeight = "View_DefaultLogicHeight";
const QString GlobalSettings::Key_View_ShowHoverMarker = "View_ShowHoverMarker";
const QString GlobalSettings::Key_View_SnapDistance = "View_SnapDistance";
const QString GlobalSettings::Key_View_CursorFillColor = "View_CursorFillColor";
const QString GlobalSettings::Key_View_CursorShowFrequency = "View_CursorShowFrequency";
const QString GlobalSettings::Key_View_CursorShowInterval = "View_CursorShowInterval";
const QString GlobalSettings::Key_View_CursorShowSamples = "View_CursorShowSamples";
const QString GlobalSettings::Key_Dec_InitialStateConfigurable = "Dec_InitialStateConfigurable";
const QString GlobalSettings::Key_Dec_ExportFormat = "Dec_ExportFormat";
const QString GlobalSettings::Key_Dec_AlwaysShowAllRows = "Dec_AlwaysShowAllRows";
const QString GlobalSettings::Key_Log_BufferSize = "Log_BufferSize";
const QString GlobalSettings::Key_Log_NotifyOfStacktrace = "Log_NotifyOfStacktrace";

vector<GlobalSettingsInterface*> GlobalSettings::callbacks_;
bool GlobalSettings::tracking_ = false;
bool GlobalSettings::is_dark_theme_ = false;
map<QString, QVariant> GlobalSettings::tracked_changes_;
QString GlobalSettings::default_style_;
QPalette GlobalSettings::default_palette_;

GlobalSettings::GlobalSettings() :
	QSettings()
{
	beginGroup("Settings");
}

void GlobalSettings::save_internal_defaults()
{
	default_style_ = qApp->style()->objectName();
	if (default_style_.isEmpty())
		default_style_ = "fusion";

	default_palette_ = QApplication::palette();
}

void GlobalSettings::set_defaults_where_needed()
{
	if (!contains(Key_General_Language)) {
		// Determine and set default UI language
		QString language = QLocale().uiLanguages().first();  // May return e.g. en-Latn-US  // clazy:exclude=detaching-temporary
		language = language.split("-").first();

		setValue(Key_General_Language, language);
		apply_language();
	}

	// Use no theme by default
	if (!contains(Key_General_Theme))
		setValue(Key_General_Theme, 0);
	if (!contains(Key_General_Style))
		setValue(Key_General_Style, "");

	// Save setup with .sr files by default
	if (!contains(Key_General_SaveWithSetup))
		setValue(Key_General_SaveWithSetup, true);

	// Enable zoom-to-fit after acquisition by default
	if (!contains(Key_View_ZoomToFitAfterAcq))
		setValue(Key_View_ZoomToFitAfterAcq, true);

	// Enable colored trace backgrounds by default
	if (!contains(Key_View_ColoredBG))
		setValue(Key_View_ColoredBG, true);

	// Enable showing sampling points by default
	if (!contains(Key_View_ShowSamplingPoints))
		setValue(Key_View_ShowSamplingPoints, true);

	// Enable filling logic signal high areas by default
	if (!contains(Key_View_FillSignalHighAreas))
		setValue(Key_View_FillSignalHighAreas, true);

	if (!contains(Key_View_DefaultDivHeight))
		setValue(Key_View_DefaultDivHeight,
		3 * QFontMetrics(QApplication::font()).height());

	if (!contains(Key_View_DefaultLogicHeight))
		setValue(Key_View_DefaultLogicHeight,
		2 * QFontMetrics(QApplication::font()).height());

	if (!contains(Key_View_ShowHoverMarker))
		setValue(Key_View_ShowHoverMarker, true);

	if (!contains(Key_View_SnapDistance))
		setValue(Key_View_SnapDistance, 15);

	if (!contains(Key_View_CursorShowInterval))
		setValue(Key_View_CursorShowInterval, true);

	if (!contains(Key_View_CursorShowFrequency))
		setValue(Key_View_CursorShowFrequency, true);

	// %c was used for the row name in the past so we need to transition such users
	if (!contains(Key_Dec_ExportFormat) ||
		value(Key_Dec_ExportFormat).toString() == "%s %d: %c: %1")
		setValue(Key_Dec_ExportFormat, "%s %d: %r: %1");

	// Default to 500 lines of backlog
	if (!contains(Key_Log_BufferSize))
		setValue(Key_Log_BufferSize, 500);

	// Notify user of existing stack trace by default
	if (!contains(Key_Log_NotifyOfStacktrace))
		setValue(Key_Log_NotifyOfStacktrace, true);

	// Default theme is bright, so use its color scheme if undefined
	if (!contains(Key_View_CursorFillColor))
		set_bright_theme_default_colors();
}

void GlobalSettings::set_bright_theme_default_colors()
{
	setValue(Key_View_FillSignalHighAreaColor,
		QColor(0, 0, 0, 5 * 256 / 100).rgba());

	setValue(Key_View_CursorFillColor,
		QColor(220, 231, 243).rgba());
}

void GlobalSettings::set_dark_theme_default_colors()
{
	setValue(Key_View_FillSignalHighAreaColor,
		QColor(188, 188, 188, 9 * 256 / 100).rgba());

	setValue(Key_View_CursorFillColor,
		QColor(60, 60, 60).rgba());
}

bool GlobalSettings::current_theme_is_dark()
{
	return is_dark_theme_;
}

void GlobalSettings::apply_theme()
{
	QString theme_name    = Themes.at(value(Key_General_Theme).toInt()).first;
	QString resource_name = Themes.at(value(Key_General_Theme).toInt()).second;

	if (!resource_name.isEmpty()) {
		QFile file(resource_name);
		file.open(QFile::ReadOnly | QFile::Text);
		qApp->setStyleSheet(file.readAll());
	} else
		qApp->setStyleSheet("");

	qApp->setPalette(default_palette_);

	const QString style = value(Key_General_Style).toString();
	if (style.isEmpty())
		qApp->setStyle(default_style_);
	else
		qApp->setStyle(style);

	is_dark_theme_ = false;

	if (theme_name.compare("QDarkStyleSheet") == 0) {
		QPalette dark_palette;
		dark_palette.setColor(QPalette::Window, QColor(53, 53, 53));
		dark_palette.setColor(QPalette::WindowText, Qt::white);
		dark_palette.setColor(QPalette::Base, QColor(42, 42, 42));
		dark_palette.setColor(QPalette::Dark, QColor(35, 35, 35));
		dark_palette.setColor(QPalette::Highlight, QColor(42, 130, 218));
		qApp->setPalette(dark_palette);
		is_dark_theme_ = true;
	} else if (theme_name.compare("DarkStyle") == 0) {
		QPalette dark_palette;
		dark_palette.setColor(QPalette::Window, QColor(53, 53, 53));
		dark_palette.setColor(QPalette::WindowText, Qt::white);
		dark_palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(127, 127, 127));
		dark_palette.setColor(QPalette::Base, QColor(42, 42, 42));
		dark_palette.setColor(QPalette::AlternateBase, QColor(66, 66, 66));
		dark_palette.setColor(QPalette::ToolTipBase, Qt::white);
		dark_palette.setColor(QPalette::ToolTipText, QColor(53, 53, 53));
		dark_palette.setColor(QPalette::Text, Qt::white);
		dark_palette.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 127, 127));
		dark_palette.setColor(QPalette::Dark, QColor(35, 35, 35));
		dark_palette.setColor(QPalette::Shadow, QColor(20, 20, 20));
		dark_palette.setColor(QPalette::Button, QColor(53, 53, 53));
		dark_palette.setColor(QPalette::ButtonText, Qt::white);
		dark_palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(127, 127, 127));
		dark_palette.setColor(QPalette::BrightText, Qt::red);
		dark_palette.setColor(QPalette::Link, QColor(42, 130, 218));
		dark_palette.setColor(QPalette::Highlight, QColor(42, 130, 218));
		dark_palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(80, 80, 80));
		dark_palette.setColor(QPalette::HighlightedText, Qt::white);
		dark_palette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(127, 127, 127));
		qApp->setPalette(dark_palette);
		is_dark_theme_ = true;
	}

	QPixmapCache::clear();
}

void GlobalSettings::apply_language()
{
	Application* a = qobject_cast<Application*>(QApplication::instance());
	a->switch_language(value(Key_General_Language).toString());
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

	// TODO Emulate noquote()
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

	for (auto& entry : tracked_changes_)
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

void GlobalSettings::store_variantbase(QSettings &settings, Glib::VariantBase v)
{
	const QByteArray var_data = QByteArray((const char*)v.get_data(), v.get_size());

	settings.setValue("value", var_data);
	settings.setValue("type", QString::fromStdString(v.get_type_string()));
}

Glib::VariantBase GlobalSettings::restore_variantbase(QSettings &settings)
{
	QString raw_type = settings.value("type").toString();
	GVariantType *var_type = g_variant_type_new(raw_type.toUtf8());

	QByteArray data = settings.value("value").toByteArray();

	gpointer var_data = g_memdup((gconstpointer)data.constData(),
		(guint)data.size());

	GVariant *value = g_variant_new_from_data(var_type, var_data,
		data.size(), false, g_free, var_data);

	Glib::VariantBase ret_val = Glib::VariantBase(value, true);

	g_variant_type_free(var_type);
	g_variant_unref(value);

	return ret_val;
}

void GlobalSettings::store_timestamp(QSettings &settings, const char *name, const pv::util::Timestamp &ts)
{
	stringstream ss;
	boost::archive::text_oarchive oa(ss);
	oa << boost::serialization::make_nvp(name, ts);
	settings.setValue(name, QString::fromStdString(ss.str()));
}

pv::util::Timestamp GlobalSettings::restore_timestamp(QSettings &settings, const char *name)
{
	util::Timestamp result;
	stringstream ss;
	ss << settings.value(name).toString().toStdString();

	try {
		boost::archive::text_iarchive ia(ss);
		ia >> boost::serialization::make_nvp(name, result);
	} catch (boost::archive::archive_exception&) {
		qDebug() << "Could not restore setting" << name;
	}

	return result;
}

} // namespace pv
