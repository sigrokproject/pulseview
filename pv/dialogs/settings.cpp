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

#include "config.h"

#include <glib.h>

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QSpinBox>
#include <QString>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTextStream>
#include <QVBoxLayout>

#include "settings.hpp"

#include "pv/application.hpp"
#include "pv/devicemanager.hpp"
#include "pv/globalsettings.hpp"
#include "pv/logging.hpp"
#include "pv/widgets/colorbutton.hpp"

#include <libsigrokcxx/libsigrokcxx.hpp>

#ifdef ENABLE_DECODE
#include <libsigrokdecode/libsigrokdecode.h>
#endif

using std::shared_ptr;
using pv::widgets::ColorButton;

namespace pv {
namespace dialogs {

/**
 * Special version of a QListView that has the width of the first column as minimum size.
 *
 * @note Inspired by https://github.com/qt-creator/qt-creator/blob/master/src/plugins/coreplugin/dialogs/settingsdialog.cpp
 */
class PageListWidget: public QListWidget
{
public:
	PageListWidget() :
		QListWidget()
	{
		setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding);
	}

	QSize sizeHint() const final
	{
		int width = sizeHintForColumn(0) + frameWidth() * 2 + 5;
		if (verticalScrollBar()->isVisible())
			width += verticalScrollBar()->width();
		return QSize(width, 100);
	}
};

Settings::Settings(DeviceManager &device_manager, QWidget *parent) :
	QDialog(parent, nullptr),
	device_manager_(device_manager)
{
	resize(600, 400);

	// Create log view
	log_view_ = create_log_view();

	// Create pages
	page_list = new PageListWidget();
	page_list->setViewMode(QListView::ListMode);
	page_list->setMovement(QListView::Static);
	page_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	pages = new QStackedWidget;
	create_pages();
	page_list->setCurrentIndex(page_list->model()->index(0, 0));

	// Create the rest of the dialog
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
	// General page
	pages->addWidget(get_general_settings_form(pages));

	QListWidgetItem *generalButton = new QListWidgetItem(page_list);
	generalButton->setIcon(QIcon(":/icons/settings-general.png"));
	generalButton->setText(tr("General"));
	generalButton->setTextAlignment(Qt::AlignVCenter);
	generalButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

	// View page
	pages->addWidget(get_view_settings_form(pages));

	QListWidgetItem *viewButton = new QListWidgetItem(page_list);
	viewButton->setIcon(QIcon(":/icons/settings-views.svg"));
	viewButton->setText(tr("Views"));
	viewButton->setTextAlignment(Qt::AlignVCenter);
	viewButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

#ifdef ENABLE_DECODE
	// Decoder page
	pages->addWidget(get_decoder_settings_form(pages));

	QListWidgetItem *decoderButton = new QListWidgetItem(page_list);
	decoderButton->setIcon(QIcon(":/icons/add-decoder.svg"));
	decoderButton->setText(tr("Decoders"));
	decoderButton->setTextAlignment(Qt::AlignVCenter);
	decoderButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
#endif

	// About page
	pages->addWidget(get_about_page(pages));

	QListWidgetItem *aboutButton = new QListWidgetItem(page_list);
	aboutButton->setIcon(QIcon(":/icons/information.svg"));
	aboutButton->setText(tr("About"));
	aboutButton->setTextAlignment(Qt::AlignVCenter);
	aboutButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

	// Logging page
	pages->addWidget(get_logging_page(pages));

	QListWidgetItem *loggingButton = new QListWidgetItem(page_list);
	loggingButton->setIcon(QIcon(":/icons/information.svg"));
	loggingButton->setText(tr("Logging"));
	loggingButton->setTextAlignment(Qt::AlignVCenter);
	loggingButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
}

QCheckBox *Settings::create_checkbox(const QString& key, const char* slot) const
{
	GlobalSettings settings;

	QCheckBox *cb = new QCheckBox();
	cb->setChecked(settings.value(key).toBool());
	connect(cb, SIGNAL(stateChanged(int)), this, slot);
	return cb;
}

QPlainTextEdit *Settings::create_log_view() const
{
	GlobalSettings settings;

	QPlainTextEdit *log_view = new QPlainTextEdit();

	log_view->setReadOnly(true);
	log_view->setWordWrapMode(QTextOption::NoWrap);
	log_view->setCenterOnScroll(true);

	log_view->appendHtml(logging.get_log());
	connect(&logging, SIGNAL(logged_text(QString)),
		log_view, SLOT(appendHtml(QString)));

	return log_view;
}

QWidget *Settings::get_general_settings_form(QWidget *parent) const
{
	GlobalSettings settings;

	QWidget *form = new QWidget(parent);
	QVBoxLayout *form_layout = new QVBoxLayout(form);

	// General settings
	QGroupBox *general_group = new QGroupBox(tr("General"));
	form_layout->addWidget(general_group);

	QFormLayout *general_layout = new QFormLayout();
	general_group->setLayout(general_layout);

	QComboBox *theme_cb = new QComboBox();
	for (pair<QString, QString> entry : Themes)
		theme_cb->addItem(entry.first, entry.second);

	theme_cb->setCurrentIndex(
		settings.value(GlobalSettings::Key_General_Theme).toInt());
	connect(theme_cb, SIGNAL(currentIndexChanged(int)),
		this, SLOT(on_general_theme_changed_changed(int)));
	general_layout->addRow(tr("User interface theme"), theme_cb);

	QLabel *description_1 = new QLabel(tr("(You may need to restart PulseView for all UI elements to update)"));
	description_1->setAlignment(Qt::AlignRight);
	general_layout->addRow(description_1);

	return form;
}

QWidget *Settings::get_view_settings_form(QWidget *parent) const
{
	GlobalSettings settings;
	QCheckBox *cb;

	QWidget *form = new QWidget(parent);
	QVBoxLayout *form_layout = new QVBoxLayout(form);

	// Trace view settings
	QGroupBox *trace_view_group = new QGroupBox(tr("Trace View"));
	form_layout->addWidget(trace_view_group);

	QFormLayout *trace_view_layout = new QFormLayout();
	trace_view_group->setLayout(trace_view_layout);

	cb = create_checkbox(GlobalSettings::Key_View_ColoredBG,
		SLOT(on_view_coloredBG_changed(int)));
	trace_view_layout->addRow(tr("Use colored trace &background"), cb);

	cb = create_checkbox(GlobalSettings::Key_View_ZoomToFitDuringAcq,
		SLOT(on_view_zoomToFitDuringAcq_changed(int)));
	trace_view_layout->addRow(tr("Constantly perform &zoom-to-fit during acquisition"), cb);

	cb = create_checkbox(GlobalSettings::Key_View_ZoomToFitAfterAcq,
		SLOT(on_view_zoomToFitAfterAcq_changed(int)));
	trace_view_layout->addRow(tr("Perform a zoom-to-&fit when acquisition stops"), cb);

	cb = create_checkbox(GlobalSettings::Key_View_TriggerIsZeroTime,
		SLOT(on_view_triggerIsZero_changed(int)));
	trace_view_layout->addRow(tr("Show time zero at the trigger"), cb);

	cb = create_checkbox(GlobalSettings::Key_View_StickyScrolling,
		SLOT(on_view_stickyScrolling_changed(int)));
	trace_view_layout->addRow(tr("Always keep &newest samples at the right edge during capture"), cb);

	cb = create_checkbox(GlobalSettings::Key_View_ShowSamplingPoints,
		SLOT(on_view_showSamplingPoints_changed(int)));
	trace_view_layout->addRow(tr("Show data &sampling points"), cb);

	cb = create_checkbox(GlobalSettings::Key_View_FillSignalHighAreas,
		SLOT(on_view_fillSignalHighAreas_changed(int)));
	trace_view_layout->addRow(tr("Fill high areas of logic signals"), cb);

	ColorButton* high_fill_cb = new ColorButton(parent);
	high_fill_cb->set_color(QColor::fromRgba(
		settings.value(GlobalSettings::Key_View_FillSignalHighAreaColor).value<uint32_t>()));
	connect(high_fill_cb, SIGNAL(selected(QColor)),
		this, SLOT(on_view_fillSignalHighAreaColor_changed(QColor)));
	trace_view_layout->addRow(tr("Fill high areas of logic signals"), high_fill_cb);

	cb = create_checkbox(GlobalSettings::Key_View_ShowAnalogMinorGrid,
		SLOT(on_view_showAnalogMinorGrid_changed(int)));
	trace_view_layout->addRow(tr("Show analog minor grid in addition to div grid"), cb);

	cb = create_checkbox(GlobalSettings::Key_View_ShowHoverMarker,
		SLOT(on_view_showHoverMarker_changed(int)));
	trace_view_layout->addRow(tr("Highlight mouse cursor using a vertical marker line"), cb);

	QSpinBox *snap_distance_sb = new QSpinBox();
	snap_distance_sb->setRange(0, 1000);
	snap_distance_sb->setSuffix(tr(" pixels"));
	snap_distance_sb->setValue(
		settings.value(GlobalSettings::Key_View_SnapDistance).toInt());
	connect(snap_distance_sb, SIGNAL(valueChanged(int)), this,
		SLOT(on_view_snapDistance_changed(int)));
	trace_view_layout->addRow(tr("Maximum distance from edges before cursors snap to them"), snap_distance_sb);

	QComboBox *thr_disp_mode_cb = new QComboBox();
	thr_disp_mode_cb->addItem(tr("None"), GlobalSettings::ConvThrDispMode_None);
	thr_disp_mode_cb->addItem(tr("Background"), GlobalSettings::ConvThrDispMode_Background);
	thr_disp_mode_cb->addItem(tr("Dots"), GlobalSettings::ConvThrDispMode_Dots);
	thr_disp_mode_cb->setCurrentIndex(
		settings.value(GlobalSettings::Key_View_ConversionThresholdDispMode).toInt());
	connect(thr_disp_mode_cb, SIGNAL(currentIndexChanged(int)),
		this, SLOT(on_view_conversionThresholdDispMode_changed(int)));
	trace_view_layout->addRow(tr("Conversion threshold display mode (analog traces only)"), thr_disp_mode_cb);

	QSpinBox *default_div_height_sb = new QSpinBox();
	default_div_height_sb->setRange(20, 1000);
	default_div_height_sb->setSuffix(tr(" pixels"));
	default_div_height_sb->setValue(
		settings.value(GlobalSettings::Key_View_DefaultDivHeight).toInt());
	connect(default_div_height_sb, SIGNAL(valueChanged(int)), this,
		SLOT(on_view_defaultDivHeight_changed(int)));
	trace_view_layout->addRow(tr("Default analog trace div height"), default_div_height_sb);

	QSpinBox *default_logic_height_sb = new QSpinBox();
	default_logic_height_sb->setRange(5, 1000);
	default_logic_height_sb->setSuffix(tr(" pixels"));
	default_logic_height_sb->setValue(
		settings.value(GlobalSettings::Key_View_DefaultLogicHeight).toInt());
	connect(default_logic_height_sb, SIGNAL(valueChanged(int)), this,
		SLOT(on_view_defaultLogicHeight_changed(int)));
	trace_view_layout->addRow(tr("Default logic trace height"), default_logic_height_sb);

	return form;
}

QWidget *Settings::get_decoder_settings_form(QWidget *parent)
{
#ifdef ENABLE_DECODE
	GlobalSettings settings;
	QCheckBox *cb;

	QWidget *form = new QWidget(parent);
	QVBoxLayout *form_layout = new QVBoxLayout(form);

	// Decoder settings
	QGroupBox *decoder_group = new QGroupBox(tr("Decoders"));
	form_layout->addWidget(decoder_group);

	QFormLayout *decoder_layout = new QFormLayout();
	decoder_group->setLayout(decoder_layout);

	cb = create_checkbox(GlobalSettings::Key_Dec_InitialStateConfigurable,
		SLOT(on_dec_initialStateConfigurable_changed(int)));
	decoder_layout->addRow(tr("Allow configuration of &initial signal state"), cb);

	// Annotation export settings
	ann_export_format_ = new QLineEdit();
	ann_export_format_->setText(
		settings.value(GlobalSettings::Key_Dec_ExportFormat).toString());
	connect(ann_export_format_, SIGNAL(textChanged(const QString&)),
		this, SLOT(on_dec_exportFormat_changed(const QString&)));
	decoder_layout->addRow(tr("Annotation export format"), ann_export_format_);
	QLabel *description_1 = new QLabel(tr("%s = sample range; %d: decoder name; %c: row name; %q: use quotations marks"));
	description_1->setAlignment(Qt::AlignRight);
	decoder_layout->addRow(description_1);
	QLabel *description_2 = new QLabel(tr("%1: longest annotation text; %a: all annotation texts"));
	description_2->setAlignment(Qt::AlignRight);
	decoder_layout->addRow(description_2);

	return form;
#else
	(void)parent;
	return nullptr;
#endif
}

QWidget *Settings::get_about_page(QWidget *parent) const
{
	Application* a = qobject_cast<Application*>(QApplication::instance());

	QLabel *icon = new QLabel();
	icon->setPixmap(QPixmap(QString::fromUtf8(":/icons/pulseview.svg")));

	// Setup the license field with the project homepage link
	QLabel *gpl_home_info = new QLabel();
	gpl_home_info->setText(tr("%1<br /><a href=\"http://%2\">%2</a>").arg(
		tr("GNU GPL, version 3 or later"),
		QApplication::organizationDomain()));
	gpl_home_info->setOpenExternalLinks(true);

	QString s;

	s.append("<style type=\"text/css\"> tr .id { white-space: pre; padding-right: 5px; } </style>");

	s.append("<table>");

	s.append("<tr><td colspan=\"2\"><b>" +
		tr("Versions, libraries and features:") + "</b></td></tr>");
	for (pair<QString, QString> &entry : a->get_version_info())
		s.append(QString("<tr><td><i>%1</i></td><td>%2</td></tr>")
			.arg(entry.first, entry.second));

	s.append("<tr><td colspan=\"2\"></td></tr>");
	s.append("<tr><td colspan=\"2\"><b>" +
		tr("Firmware search paths:") + "</b></td></tr>");
	for (QString &entry : a->get_fw_path_list())
		s.append(QString("<tr><td colspan=\"2\">%1</td></tr>").arg(entry));

#ifdef ENABLE_DECODE
	s.append("<tr><td colspan=\"2\"></td></tr>");
	s.append("<tr><td colspan=\"2\"><b>" +
		tr("Protocol decoder search paths:") + "</b></td></tr>");
	for (QString &entry : a->get_pd_path_list())
		s.append(QString("<tr><td colspan=\"2\">%1</td></tr>").arg(entry));
#endif

	s.append("<tr><td colspan=\"2\"></td></tr>");
	s.append("<tr><td colspan=\"2\"><b>" +
		tr("Supported hardware drivers:") + "</b></td></tr>");
	for (pair<QString, QString> &entry : a->get_driver_list())
		s.append(QString("<tr><td class=\"id\"><i>%1</i></td><td>%2</td></tr>")
			.arg(entry.first, entry.second));

	s.append("<tr><td colspan=\"2\"></td></tr>");
	s.append("<tr><td colspan=\"2\"><b>" +
		tr("Supported input formats:") + "</b></td></tr>");
	for (pair<QString, QString> &entry : a->get_input_format_list())
		s.append(QString("<tr><td class=\"id\"><i>%1</i></td><td>%2</td></tr>")
			.arg(entry.first, entry.second));

	s.append("<tr><td colspan=\"2\"></td></tr>");
	s.append("<tr><td colspan=\"2\"><b>" +
		tr("Supported output formats:") + "</b></td></tr>");
	for (pair<QString, QString> &entry : a->get_output_format_list())
		s.append(QString("<tr><td class=\"id\"><i>%1</i></td><td>%2</td></tr>")
			.arg(entry.first, entry.second));

#ifdef ENABLE_DECODE
	s.append("<tr><td colspan=\"2\"></td></tr>");
	s.append("<tr><td colspan=\"2\"><b>" +
		tr("Supported protocol decoders:") + "</b></td></tr>");
	for (pair<QString, QString> &entry : a->get_pd_list())
		s.append(QString("<tr><td class=\"id\"><i>%1</i></td><td>%2</td></tr>")
			.arg(entry.first, entry.second));
#endif

	s.append("</table>");

	QTextDocument *supported_doc = new QTextDocument();
	supported_doc->setHtml(s);

	QTextBrowser *support_list = new QTextBrowser();
	support_list->setDocument(supported_doc);

	QHBoxLayout *h_layout = new QHBoxLayout();
	h_layout->setAlignment(Qt::AlignLeft);
	h_layout->addWidget(icon);
	h_layout->addWidget(gpl_home_info);

	QVBoxLayout *layout = new QVBoxLayout();
	layout->addLayout(h_layout);
	layout->addWidget(support_list);

	QWidget *page = new QWidget(parent);
	page->setLayout(layout);

	return page;
}

QWidget *Settings::get_logging_page(QWidget *parent) const
{
	GlobalSettings settings;

	// Log level
	QSpinBox *loglevel_sb = new QSpinBox();
	loglevel_sb->setMaximum(SR_LOG_SPEW);
	loglevel_sb->setValue(logging.get_log_level());
	connect(loglevel_sb, SIGNAL(valueChanged(int)), this,
		SLOT(on_log_logLevel_changed(int)));

	QHBoxLayout *loglevel_layout = new QHBoxLayout();
	loglevel_layout->addWidget(new QLabel(tr("Log level:")));
	loglevel_layout->addWidget(loglevel_sb);

	// Background buffer size
	QSpinBox *buffersize_sb = new QSpinBox();
	buffersize_sb->setSuffix(tr(" lines"));
	buffersize_sb->setMinimum(Logging::MIN_BUFFER_SIZE);
	buffersize_sb->setMaximum(Logging::MAX_BUFFER_SIZE);
	buffersize_sb->setValue(
		settings.value(GlobalSettings::Key_Log_BufferSize).toInt());
	connect(buffersize_sb, SIGNAL(valueChanged(int)), this,
		SLOT(on_log_bufferSize_changed(int)));

	QHBoxLayout *buffersize_layout = new QHBoxLayout();
	buffersize_layout->addWidget(new QLabel(tr("Length of background buffer:")));
	buffersize_layout->addWidget(buffersize_sb);

	// Save to file
	QPushButton *save_log_pb = new QPushButton(
		QIcon::fromTheme("document-save-as", QIcon(":/icons/document-save-as.png")),
		tr("&Save to File"));
	connect(save_log_pb, SIGNAL(clicked(bool)),
		this, SLOT(on_log_saveToFile_clicked(bool)));

	// Pop out
	QPushButton *pop_out_pb = new QPushButton(
		QIcon::fromTheme("window-new", QIcon(":/icons/window-new.png")),
		tr("&Pop out"));
	connect(pop_out_pb, SIGNAL(clicked(bool)),
		this, SLOT(on_log_popOut_clicked(bool)));

	QHBoxLayout *control_layout = new QHBoxLayout();
	control_layout->addLayout(loglevel_layout);
	control_layout->addLayout(buffersize_layout);
	control_layout->addWidget(save_log_pb);
	control_layout->addWidget(pop_out_pb);

	QVBoxLayout *root_layout = new QVBoxLayout();
	root_layout->addLayout(control_layout);
	root_layout->addWidget(log_view_);

	QWidget *page = new QWidget(parent);
	page->setLayout(root_layout);

	return page;
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

void Settings::on_general_theme_changed_changed(int state)
{
	GlobalSettings settings;
	settings.setValue(GlobalSettings::Key_General_Theme, state);
	settings.apply_theme();
}

void Settings::on_view_zoomToFitDuringAcq_changed(int state)
{
	GlobalSettings settings;
	settings.setValue(GlobalSettings::Key_View_ZoomToFitDuringAcq, state ? true : false);
}

void Settings::on_view_zoomToFitAfterAcq_changed(int state)
{
	GlobalSettings settings;
	settings.setValue(GlobalSettings::Key_View_ZoomToFitAfterAcq, state ? true : false);
}

void Settings::on_view_triggerIsZero_changed(int state)
{
	GlobalSettings settings;
	settings.setValue(GlobalSettings::Key_View_TriggerIsZeroTime, state ? true : false);
}

void Settings::on_view_coloredBG_changed(int state)
{
	GlobalSettings settings;
	settings.setValue(GlobalSettings::Key_View_ColoredBG, state ? true : false);
}

void Settings::on_view_stickyScrolling_changed(int state)
{
	GlobalSettings settings;
	settings.setValue(GlobalSettings::Key_View_StickyScrolling, state ? true : false);
}

void Settings::on_view_showSamplingPoints_changed(int state)
{
	GlobalSettings settings;
	settings.setValue(GlobalSettings::Key_View_ShowSamplingPoints, state ? true : false);
}

void Settings::on_view_fillSignalHighAreas_changed(int state)
{
	GlobalSettings settings;
	settings.setValue(GlobalSettings::Key_View_FillSignalHighAreas, state ? true : false);
}

void Settings::on_view_fillSignalHighAreaColor_changed(QColor color)
{
	GlobalSettings settings;
	settings.setValue(GlobalSettings::Key_View_FillSignalHighAreaColor, color.rgba());
}

void Settings::on_view_showAnalogMinorGrid_changed(int state)
{
	GlobalSettings settings;
	settings.setValue(GlobalSettings::Key_View_ShowAnalogMinorGrid, state ? true : false);
}

void Settings::on_view_showHoverMarker_changed(int state)
{
	GlobalSettings settings;
	settings.setValue(GlobalSettings::Key_View_ShowHoverMarker, state ? true : false);
}

void Settings::on_view_snapDistance_changed(int value)
{
	GlobalSettings settings;
	settings.setValue(GlobalSettings::Key_View_SnapDistance, value);
}

void Settings::on_view_conversionThresholdDispMode_changed(int state)
{
	GlobalSettings settings;
	settings.setValue(GlobalSettings::Key_View_ConversionThresholdDispMode, state);
}

void Settings::on_view_defaultDivHeight_changed(int value)
{
	GlobalSettings settings;
	settings.setValue(GlobalSettings::Key_View_DefaultDivHeight, value);
}

void Settings::on_view_defaultLogicHeight_changed(int value)
{
	GlobalSettings settings;
	settings.setValue(GlobalSettings::Key_View_DefaultLogicHeight, value);
}

#ifdef ENABLE_DECODE
void Settings::on_dec_initialStateConfigurable_changed(int state)
{
	GlobalSettings settings;
	settings.setValue(GlobalSettings::Key_Dec_InitialStateConfigurable, state ? true : false);
}

void Settings::on_dec_exportFormat_changed(const QString &text)
{
	GlobalSettings settings;
	settings.setValue(GlobalSettings::Key_Dec_ExportFormat, text);
}
#endif

void Settings::on_log_logLevel_changed(int value)
{
	logging.set_log_level(value);
}

void Settings::on_log_bufferSize_changed(int value)
{
	GlobalSettings settings;
	settings.setValue(GlobalSettings::Key_Log_BufferSize, value);
}

void Settings::on_log_saveToFile_clicked(bool checked)
{
	(void)checked;

	const QString file_name = QFileDialog::getSaveFileName(
		this, tr("Save Log"), "", tr("Log Files (*.txt *.log);;All Files (*)"));

	if (file_name.isEmpty())
		return;

	QFile file(file_name);
	if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
		QTextStream out_stream(&file);
		out_stream << log_view_->toPlainText();

		if (out_stream.status() == QTextStream::Ok) {
			QMessageBox msg(this);
			msg.setText(tr("Success"));
			msg.setInformativeText(tr("Log saved to %1.").arg(file_name));
			msg.setStandardButtons(QMessageBox::Ok);
			msg.setIcon(QMessageBox::Information);
			msg.exec();

			return;
		}
	}

	QMessageBox msg(this);
	msg.setText(tr("Error"));
	msg.setInformativeText(tr("File %1 could not be written to.").arg(file_name));
	msg.setStandardButtons(QMessageBox::Ok);
	msg.setIcon(QMessageBox::Warning);
	msg.exec();
}

void Settings::on_log_popOut_clicked(bool checked)
{
	(void)checked;

	// Create the window as a sub-window so it closes when the main window closes
	QMainWindow *window = new QMainWindow(nullptr, Qt::SubWindow);

	window->setObjectName(QString::fromUtf8("Log Window"));
	window->setWindowTitle(tr("%1 Log").arg(PV_TITLE));

	// Use same width/height as the settings dialog
	window->resize(width(), height());

	window->setCentralWidget(create_log_view());
	window->show();
}

} // namespace dialogs
} // namespace pv
