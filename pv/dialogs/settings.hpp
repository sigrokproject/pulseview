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

#ifndef PULSEVIEW_PV_DIALOGS_SETTINGS_HPP
#define PULSEVIEW_PV_DIALOGS_SETTINGS_HPP

#include <QCheckBox>
#include <QColor>
#include <QDialog>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QStackedWidget>
#include <QLineEdit>
#include <QComboBox>

#define NAV_SETTINGS_FUNC_DECLARE1(name)			\
	void on_nav_ ## name ## Type_changed(int type);	\
	void on_nav_ ## name ## Amount_changed(const QString& astr)

#define NAV_SETTINGS_FUNC_DECLARE(name)		\
	NAV_SETTINGS_FUNC_DECLARE1(name);		\
	NAV_SETTINGS_FUNC_DECLARE1(name ## Alt);\
	NAV_SETTINGS_FUNC_DECLARE1(name ## Ctrl);\
	NAV_SETTINGS_FUNC_DECLARE1(name ## Shift)

#define NAV_SETTINGS_FUNC_DEFINE1(name)				\
	void Settings::on_nav_ ## name ## Type_changed(int type){	\
		GlobalSettings settings;	\
		settings.setValue(GlobalSettings::Key_Nav_ ## name ## Type, type);	\
	}	\
	void Settings::on_nav_ ## name ## Amount_changed(const QString& astr) {	\
		GlobalSettings settings;	\
		settings.setValue(GlobalSettings::Key_Nav_ ## name ## Amount, astr.toDouble());	\
	}

#define NAV_SETTINGS_FUNC_DEFINE(name)		\
	NAV_SETTINGS_FUNC_DEFINE1(name)			\
	NAV_SETTINGS_FUNC_DEFINE1(name ## Alt)	\
	NAV_SETTINGS_FUNC_DEFINE1(name ## Ctrl)	\
	NAV_SETTINGS_FUNC_DEFINE1(name ## Shift)

#define NAV_SETTINGS_ROW(name, text, row)			\
	QLabel * name ## _label = new QLabel(tr(text));\
	name ## _label->setAlignment(Qt::AlignLeft);	\
	nav_layout->addWidget(name ## _label, row, 0);	\
	\
	name ## _type_cb_ = new QComboBox();	\
	name ## _type_cb_->addItem(tr("None"), NAV_TYPE_NONE);	\
	name ## _type_cb_->addItem(tr("Zoom"), NAV_TYPE_ZOOM);	\
	name ## _type_cb_->addItem(tr("Move Horizontally"), NAV_TYPE_HORI);	\
	name ## _type_cb_->addItem(tr("Move Vertically"), NAV_TYPE_VERT);	\
	name ## _type_cb_->setCurrentIndex( settings.value(GlobalSettings::Key_Nav_ ## name ## Type).toInt() );	\
	connect(name ## _type_cb_, SIGNAL(currentIndexChanged(int)), this, SLOT(on_nav_ ## name ## Type_changed(int)));	\
	nav_layout->addWidget(name ## _type_cb_, row, 1);	\
	\
	name ## _amount_edit_ = new QLineEdit();	\
	name ## _amount_edit_->setText( settings.value(GlobalSettings::Key_Nav_ ## name ## Amount).toString() );	\
	connect(name ## _amount_edit_, SIGNAL(textChanged(const QString&)), this, SLOT(on_nav_ ## name ## Amount_changed(const QString&)));	\
	nav_layout->addWidget(name ## _amount_edit_, row, 2)
//	name ## _amount_edit_->setValidator( new QDoubleValidator(name ## _amount_edit_) );

#define NAV_SETTINGS_ROWS(name, text, row)			\
	NAV_SETTINGS_ROW(name,          text,            row*4 + 0);	\
	NAV_SETTINGS_ROW(name ## Alt,   text " + Alt",   row*4 + 1);	\
	NAV_SETTINGS_ROW(name ## Ctrl,  text " + Ctrl",  row*4 + 2);	\
	NAV_SETTINGS_ROW(name ## Shift, text " + Shift", row*4 + 3)

#define NAV_SETTINGS_VAR_DECLARE1(name)	\
	QComboBox * name ## _type_cb_;		\
	QLineEdit* name ## _amount_edit_

#define NAV_SETTINGS_VAR_DECLARE(name)	\
	NAV_SETTINGS_VAR_DECLARE1(name);	\
	NAV_SETTINGS_VAR_DECLARE1(name ## Alt);	\
	NAV_SETTINGS_VAR_DECLARE1(name ## Ctrl);	\
	NAV_SETTINGS_VAR_DECLARE1(name ## Shift)

#define NAV_SETTINGS_LOAD1(name)			\
	name ## _type_cb_->setCurrentIndex( settings.value(GlobalSettings::Key_Nav_ ## name ## Type).toInt() );	\
	name ## _amount_edit_->setText( settings.value(GlobalSettings::Key_Nav_ ## name ## Amount).toString() )

#define NAV_SETTINGS_LOAD(name)			\
	NAV_SETTINGS_LOAD1(name);			\
	NAV_SETTINGS_LOAD1(name ## Alt);	\
	NAV_SETTINGS_LOAD1(name ## Ctrl);	\
	NAV_SETTINGS_LOAD1(name ## Shift)

namespace pv {

class DeviceManager;

namespace dialogs {

class PageListWidget;

class Settings : public QDialog
{
	Q_OBJECT

public:
	Settings(DeviceManager &device_manager, QWidget *parent = nullptr);

	void create_pages();
	QCheckBox *create_checkbox(const QString& key, const char* slot) const;
	QPlainTextEdit *create_log_view() const;

	QWidget *get_general_settings_form(QWidget *parent) const;
	QWidget *get_view_settings_form(QWidget *parent) const;
	QWidget *get_navigation_settings_form(QWidget *parent);
	QWidget *get_decoder_settings_form(QWidget *parent);
	QWidget *get_about_page(QWidget *parent) const;
	QWidget *get_logging_page(QWidget *parent) const;

	void accept();
	void reject();

private Q_SLOTS:
	void on_page_changed(QListWidgetItem *current, QListWidgetItem *previous);
	void on_general_language_changed(const QString &text);
	void on_general_theme_changed(int value);
	void on_general_style_changed(int value);
	void on_general_save_with_setup_changed(int state);
	void on_view_zoomToFitDuringAcq_changed(int state);
	void on_view_zoomToFitAfterAcq_changed(int state);
	void on_view_triggerIsZero_changed(int state);
	void on_view_coloredBG_changed(int state);
	void on_view_stickyScrolling_changed(int state);
	void on_view_showSamplingPoints_changed(int state);
	void on_view_fillSignalHighAreas_changed(int state);
	void on_view_fillSignalHighAreaColor_changed(QColor color);
	void on_view_showAnalogMinorGrid_changed(int state);
	void on_view_showHoverMarker_changed(int state);
	void on_view_snapDistance_changed(int value);
	void on_view_cursorFillColor_changed(QColor color);
	void on_view_conversionThresholdDispMode_changed(int state);
	void on_view_defaultDivHeight_changed(int value);
	void on_view_defaultLogicHeight_changed(int value);
	void on_nav_resetZoomControls_clicked(bool checked);
	void on_nav_resetMoveControls_clicked(bool checked);
	NAV_SETTINGS_FUNC_DECLARE(UpDown);
	NAV_SETTINGS_FUNC_DECLARE(LeftRight);
	NAV_SETTINGS_FUNC_DECLARE(PageUpDown);
	NAV_SETTINGS_FUNC_DECLARE(WheelHori);
	NAV_SETTINGS_FUNC_DECLARE(WheelVert);
	void nav_load_gui_from_settings();
	#ifdef ENABLE_DECODE
	void on_dec_initialStateConfigurable_changed(int state);
	void on_dec_exportFormat_changed(const QString &text);
	void on_dec_alwaysshowallrows_changed(int state);
#endif
	void on_log_logLevel_changed(int value);
	void on_log_bufferSize_changed(int value);
	void on_log_saveToFile_clicked(bool checked);
	void on_log_popOut_clicked(bool checked);

private:
	DeviceManager &device_manager_;
	PageListWidget *page_list;
	QStackedWidget *pages;
	NAV_SETTINGS_VAR_DECLARE(UpDown);
	NAV_SETTINGS_VAR_DECLARE(LeftRight);
	NAV_SETTINGS_VAR_DECLARE(PageUpDown);
	NAV_SETTINGS_VAR_DECLARE(WheelHori);
	NAV_SETTINGS_VAR_DECLARE(WheelVert);

#ifdef ENABLE_DECODE
	QLineEdit *ann_export_format_;
#endif

	QPlainTextEdit *log_view_;
};

} // namespace dialogs
} // namespace pv

#endif // PULSEVIEW_PV_DIALOGS_SETTINGS_HPP
