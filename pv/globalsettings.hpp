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

#ifndef PULSEVIEW_PV_GLOBALSETTINGS_HPP
#define PULSEVIEW_PV_GLOBALSETTINGS_HPP

#include <map>

#include <glib.h>
#include <glibmm/variant.h>

#include <QPalette>
#include <QSettings>
#include <QString>
#include <QVariant>

#include "util.hpp"

using std::map;
using std::pair;
using std::vector;

namespace pv {

extern const vector< pair<QString, QString> > Themes;


class GlobalSettingsInterface
{
public:
	virtual void on_setting_changed(const QString &key, const QVariant &value) = 0;
};


// type of navigation to perform
#define NAV_TYPE_NONE		0
#define NAV_TYPE_ZOOM		1
#define NAV_TYPE_HORI		2
#define NAV_TYPE_VERT		3

#define NAV_GSETTINGS_VAR_DECLARE(name)						\
	static const QString Key_Nav_ ## name ## Type;			\
	static const QString Key_Nav_ ## name ## Amount;		\
	static const QString Key_Nav_ ## name ## AltType;		\
	static const QString Key_Nav_ ## name ## AltAmount;		\
	static const QString Key_Nav_ ## name ## CtrlType;		\
	static const QString Key_Nav_ ## name ## CtrlAmount;	\
	static const QString Key_Nav_ ## name ## ShiftType;		\
	static const QString Key_Nav_ ## name ## ShiftAmount

#define NAV_GSETTINGS_VAR_SETUP(name)						\
	const QString GlobalSettings::Key_Nav_ ## name ## Type = "Nav_" #name "Type";				\
	const QString GlobalSettings::Key_Nav_ ## name ## Amount = "Nav_" #name "Amount";			\
	const QString GlobalSettings::Key_Nav_ ## name ## AltType = "Nav_" #name "AltType";			\
	const QString GlobalSettings::Key_Nav_ ## name ## AltAmount = "Nav_" #name "AltAmount";		\
	const QString GlobalSettings::Key_Nav_ ## name ## CtrlType = "Nav_" #name "CtrlType";		\
	const QString GlobalSettings::Key_Nav_ ## name ## CtrlAmount = "Nav_" #name "CtrlAmount";	\
	const QString GlobalSettings::Key_Nav_ ## name ## ShiftType = "Nav_" #name "ShiftType";		\
	const QString GlobalSettings::Key_Nav_ ## name ## ShiftAmount = "Nav_" #name "ShiftAmount"

#define NAV_GSETTINGS_VAR_DEFAULT(name, type, amount)	\
	if (!contains(Key_Nav_ ## name ## Type) || force)	\
		setValue(Key_Nav_ ## name ## Type, type);		\
	if (!contains(Key_Nav_ ## name ## Amount) || force)	\
		setValue(Key_Nav_ ## name ## Amount, amount)


class GlobalSettings : public QSettings
{
	Q_OBJECT

public:
	static const QString Key_General_Language;
	static const QString Key_General_Theme;
	static const QString Key_General_Style;
	static const QString Key_General_SaveWithSetup;
	static const QString Key_View_ZoomToFitDuringAcq;
	static const QString Key_View_ZoomToFitAfterAcq;
	static const QString Key_View_TriggerIsZeroTime;
	static const QString Key_View_ColoredBG;
	static const QString Key_View_StickyScrolling;
	static const QString Key_View_ShowSamplingPoints;
	static const QString Key_View_FillSignalHighAreas;
	static const QString Key_View_FillSignalHighAreaColor;
	static const QString Key_View_ShowAnalogMinorGrid;
	static const QString Key_View_ConversionThresholdDispMode;
	static const QString Key_View_DefaultDivHeight;
	static const QString Key_View_DefaultLogicHeight;
	static const QString Key_View_ShowHoverMarker;
	static const QString Key_View_SnapDistance;
	static const QString Key_View_CursorFillColor;
	static const QString Key_View_CursorShowInterval;
	static const QString Key_View_CursorShowFrequency;
	static const QString Key_View_CursorShowSamples;
	NAV_GSETTINGS_VAR_DECLARE(UpDown);
	NAV_GSETTINGS_VAR_DECLARE(LeftRight);
	NAV_GSETTINGS_VAR_DECLARE(PageUpDown);
	NAV_GSETTINGS_VAR_DECLARE(WheelHori);
	NAV_GSETTINGS_VAR_DECLARE(WheelVert);
	static const QString Key_Dec_InitialStateConfigurable;
	static const QString Key_Dec_ExportFormat;
	static const QString Key_Dec_AlwaysShowAllRows;
	static const QString Key_Log_BufferSize;
	static const QString Key_Log_NotifyOfStacktrace;

	enum ConvThrDispMode {
		ConvThrDispMode_None = 0,
		ConvThrDispMode_Background,
		ConvThrDispMode_Dots
	};

public:
	GlobalSettings();

	void save_internal_defaults();
	void set_defaults_where_needed();
	void set_nav_zoom_defaults(bool force);
	void set_nav_move_defaults(bool force);
	void set_bright_theme_default_colors();
	void set_dark_theme_default_colors();

	static bool current_theme_is_dark();
	void apply_theme();

	void apply_language();

	static void add_change_handler(GlobalSettingsInterface *cb);
	static void remove_change_handler(GlobalSettingsInterface *cb);

	void setValue(const QString& key, const QVariant& value);

	/**
	 * Begins the tracking of changes. All changes will
	 * be recorded until stop_tracking() is called.
	 * The change tracking is global and doesn't support nesting.
	 */
	void start_tracking();

	/**
	 * Ends the tracking of changes without any changes to the settings.
	 */
	void stop_tracking();

	/**
	 * Ends the tracking of changes, undoing the changes since the
	 * change tracking began.
	 */
	void undo_tracked_changes();

	static void store_gvariant(QSettings &settings, GVariant *v);
	static GVariant* restore_gvariant(QSettings &settings);

	static void store_variantbase(QSettings &settings, Glib::VariantBase v);
	static Glib::VariantBase restore_variantbase(QSettings &settings);

	static void store_timestamp(QSettings &settings, const char *name, const pv::util::Timestamp &ts);
	static pv::util::Timestamp restore_timestamp(QSettings &settings, const char *name);

private:
	static vector<GlobalSettingsInterface*> callbacks_;

	static bool tracking_;
	static map<QString, QVariant> tracked_changes_;

	static QString default_style_;
	static QPalette default_palette_;

	static bool is_dark_theme_;
};

} // namespace pv

#endif // PULSEVIEW_PV_GLOBALSETTINGS_HPP
