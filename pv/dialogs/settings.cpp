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
#include <boost/version.hpp>

#include <QApplication>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QString>
#include <QTextBrowser>
#include <QTextDocument>
#include <QVBoxLayout>

#include "settings.hpp"

#include "pv/devicemanager.hpp"
#include "pv/globalsettings.hpp"

#include <libsigrokcxx/libsigrokcxx.hpp>

#ifdef ENABLE_DECODE
#include <libsigrokdecode/libsigrokdecode.h>
#endif

using std::shared_ptr;

namespace pv {
namespace dialogs {

Settings::Settings(DeviceManager &device_manager, QWidget *parent) :
	QDialog(parent, nullptr),
	device_manager_(device_manager)
{
	const int icon_size = 64;

	resize(600, 400);

	page_list = new QListWidget;
	page_list->setViewMode(QListView::IconMode);
	page_list->setIconSize(QSize(icon_size, icon_size));
	page_list->setMovement(QListView::Static);
	page_list->setMaximumWidth(icon_size + (icon_size / 2));
	page_list->setSpacing(12);

	pages = new QStackedWidget;
	create_pages();
	page_list->setCurrentIndex(page_list->model()->index(0, 0));

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
	viewButton->setIcon(QIcon(":/icons/settings-views.svg"));
	viewButton->setText(tr("Views"));
	viewButton->setTextAlignment(Qt::AlignHCenter);
	viewButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

#ifdef ENABLE_DECODE
	// Decoder page
	pages->addWidget(get_decoder_settings_form(pages));

	QListWidgetItem *decoderButton = new QListWidgetItem(page_list);
	decoderButton->setIcon(QIcon(":/icons/add-decoder.svg"));
	decoderButton->setText(tr("Decoders"));
	decoderButton->setTextAlignment(Qt::AlignHCenter);
	decoderButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
#endif

	// About page
	pages->addWidget(get_about_page(pages));

	QListWidgetItem *aboutButton = new QListWidgetItem(page_list);
	aboutButton->setIcon(QIcon(":/icons/information.svg"));
	aboutButton->setText(tr("About"));
	aboutButton->setTextAlignment(Qt::AlignHCenter);
	aboutButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
}

QCheckBox *Settings::create_checkbox(const QString& key, const char* slot) const
{
	GlobalSettings settings;

	QCheckBox *cb = new QCheckBox();
	cb->setChecked(settings.value(key).toBool());
	connect(cb, SIGNAL(stateChanged(int)), this, slot);
	return cb;
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

	cb = create_checkbox(GlobalSettings::Key_View_ColouredBG,
		SLOT(on_view_colouredBG_changed(int)));
	trace_view_layout->addRow(tr("Use coloured trace &background"), cb);

	cb = create_checkbox(GlobalSettings::Key_View_ZoomToFitDuringAcq,
		SLOT(on_view_zoomToFitDuringAcq_changed(int)));
	trace_view_layout->addRow(tr("Constantly perform &zoom-to-fit during acquisition"), cb);

	cb = create_checkbox(GlobalSettings::Key_View_ZoomToFitAfterAcq,
		SLOT(on_view_zoomToFitAfterAcq_changed(int)));
	trace_view_layout->addRow(tr("Perform a zoom-to-&fit when acquisition stops"), cb);

	cb = create_checkbox(GlobalSettings::Key_View_StickyScrolling,
		SLOT(on_view_stickyScrolling_changed(int)));
	trace_view_layout->addRow(tr("Always keep &newest samples at the right edge during capture"), cb);

	cb = create_checkbox(GlobalSettings::Key_View_ShowSamplingPoints,
		SLOT(on_view_showSamplingPoints_changed(int)));
	trace_view_layout->addRow(tr("Show data &sampling points"), cb);

	cb = create_checkbox(GlobalSettings::Key_View_ShowAnalogMinorGrid,
		SLOT(on_view_showAnalogMinorGrid_changed(int)));
	trace_view_layout->addRow(tr("Show analog minor grid in addition to vdiv grid"), cb);

	QSpinBox *default_div_height_sb = new QSpinBox();
	default_div_height_sb->setRange(20, 1000);
	default_div_height_sb->setSuffix(tr(" pixels"));
	default_div_height_sb->setValue(
		settings.value(GlobalSettings::Key_View_DefaultDivHeight).toInt());
	connect(default_div_height_sb, SIGNAL(valueChanged(int)), this,
		SLOT(on_view_defaultDivHeight_changed(int)));
	trace_view_layout->addRow(tr("Default analog trace div height"), default_div_height_sb);

	return form;
}

QWidget *Settings::get_decoder_settings_form(QWidget *parent) const
{
#ifdef ENABLE_DECODE
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

	return form;
#else
	(void)parent;
#endif
}

#ifdef ENABLE_DECODE
static gint sort_pds(gconstpointer a, gconstpointer b)
{
	const struct srd_decoder *sda, *sdb;

	sda = (const struct srd_decoder *)a;
	sdb = (const struct srd_decoder *)b;
	return strcmp(sda->id, sdb->id);
}
#endif

QWidget *Settings::get_about_page(QWidget *parent) const
{
#ifdef ENABLE_DECODE
	struct srd_decoder *dec;
#endif

	QLabel *icon = new QLabel();
	icon->setPixmap(QPixmap(QString::fromUtf8(":/icons/pulseview.svg")));

	/* Setup the version field */
	QLabel *version_info = new QLabel();
	version_info->setText(tr("%1 %2<br />%3<br /><a href=\"http://%4\">%4</a>")
		.arg(QApplication::applicationName(),
		QApplication::applicationVersion(),
		tr("GNU GPL, version 3 or later"),
		QApplication::organizationDomain()));
	version_info->setOpenExternalLinks(true);

	shared_ptr<sigrok::Context> context = device_manager_.context();

	QString s;

	s.append("<style type=\"text/css\"> tr .id { white-space: pre; padding-right: 5px; } </style>");

	s.append("<table>");

	/* Library info */
	s.append("<tr><td colspan=\"2\"><b>" +
		tr("Libraries and features:") + "</b></td></tr>");

	s.append(QString("<tr><td><i>%1</i></td><td>%2</td></tr>")
		.arg(QString("Qt"), qVersion()));
	s.append(QString("<tr><td><i>%1</i></td><td>%2</td></tr>")
		.arg(QString("glibmm"), PV_GLIBMM_VERSION));
	s.append(QString("<tr><td><i>%1</i></td><td>%2</td></tr>")
		.arg(QString("Boost"), BOOST_LIB_VERSION));

	s.append(QString("<tr><td><i>%1</i></td><td>%2/%3 (rt: %4/%5)</td></tr>")
		.arg(QString("libsigrok"), SR_PACKAGE_VERSION_STRING,
		SR_LIB_VERSION_STRING, sr_package_version_string_get(),
		sr_lib_version_string_get()));

	GSList *l_orig = sr_buildinfo_libs_get();
	for (GSList *l = l_orig; l; l = l->next) {
		GSList *m = (GSList *)l->data;
		const char *lib = (const char *)m->data;
		const char *version = (const char *)m->next->data;
		s.append(QString("<tr><td><i>- %1</i></td><td>%2</td></tr>")
			.arg(QString(lib), QString(version)));
		g_slist_free_full(m, g_free);
	}
	g_slist_free(l_orig);

	char *host = sr_buildinfo_host_get();
	s.append(QString("<tr><td><i>- Host</i></td><td>%1</td></tr>")
		.arg(QString(host)));
	g_free(host);

	char *scpi_backends = sr_buildinfo_scpi_backends_get();
	s.append(QString("<tr><td><i>- SCPI backends</i></td><td>%1</td></tr>")
		.arg(QString(scpi_backends)));
	g_free(scpi_backends);

#ifdef ENABLE_DECODE
	s.append(QString("<tr><td><i>%1</i></td><td>%2/%3 (rt: %4/%5)</td></tr>")
		.arg(QString("libsigrokdecode"), SRD_PACKAGE_VERSION_STRING,
		SRD_LIB_VERSION_STRING, srd_package_version_string_get(),
		srd_lib_version_string_get()));

	l_orig = srd_buildinfo_libs_get();
	for (GSList *l = l_orig; l; l = l->next) {
		GSList *m = (GSList *)l->data;
		const char *lib = (const char *)m->data;
		const char *version = (const char *)m->next->data;
		s.append(QString("<tr><td><i>- %1</i></td><td>%2</td></tr>")
			.arg(QString(lib), QString(version)));
		g_slist_free_full(m, g_free);
	}
	g_slist_free(l_orig);

	host = srd_buildinfo_host_get();
	s.append(QString("<tr><td><i>- Host</i></td><td>%1</td></tr>")
		.arg(QString(host)));
	g_free(host);
#endif

	/* Set up the supported field */
	s.append("<tr><td colspan=\"2\"></td></tr>");
	s.append("<tr><td colspan=\"2\"><b>" +
		tr("Supported hardware drivers:") + "</b></td></tr>");
	for (auto entry : context->drivers()) {
		s.append(QString("<tr><td class=\"id\"><i>%1</i></td><td>%2</td></tr>")
			.arg(QString::fromUtf8(entry.first.c_str()),
				QString::fromUtf8(entry.second->long_name().c_str())));
	}

	s.append("<tr><td colspan=\"2\"></td></tr>");
	s.append("<tr><td colspan=\"2\"><b>" +
		tr("Supported input formats:") + "</b></td></tr>");
	for (auto entry : context->input_formats()) {
		s.append(QString("<tr><td class=\"id\"><i>%1</i></td><td>%2</td></tr>")
			.arg(QString::fromUtf8(entry.first.c_str()),
				QString::fromUtf8(entry.second->description().c_str())));
	}

	s.append("<tr><td colspan=\"2\"></td></tr>");
	s.append("<tr><td colspan=\"2\"><b>" +
		tr("Supported output formats:") + "</b></td></tr>");
	for (auto entry : context->output_formats()) {
		s.append(QString("<tr><td class=\"id\"><i>%1</i></td><td>%2</td></tr>")
			.arg(QString::fromUtf8(entry.first.c_str()),
				QString::fromUtf8(entry.second->description().c_str())));
	}

#ifdef ENABLE_DECODE
	s.append("<tr><td colspan=\"2\"></td></tr>");
	s.append("<tr><td colspan=\"2\"><b>" +
		tr("Supported protocol decoders:") + "</b></td></tr>");
	GSList *sl = g_slist_copy((GSList *)srd_decoder_list());
	sl = g_slist_sort(sl, sort_pds);
	for (const GSList *l = sl; l; l = l->next) {
		dec = (struct srd_decoder *)l->data;
		s.append(QString("<tr><td class=\"id\"><i>%1</i></td><td>%2</td></tr>")
			.arg(QString::fromUtf8(dec->id),
				QString::fromUtf8(dec->longname)));
	}
	g_slist_free(sl);
#endif

	s.append("</table>");

	QTextDocument *supported_doc = new QTextDocument();
	supported_doc->setHtml(s);

	QTextBrowser *support_list = new QTextBrowser();
	support_list->setDocument(supported_doc);

	QGridLayout *layout = new QGridLayout();
	layout->addWidget(icon, 0, 0, 1, 1);
	layout->addWidget(version_info, 0, 1, 1, 1);
	layout->addWidget(support_list, 1, 1, 1, 1);

	QWidget *page = new QWidget(parent);
	page->setLayout(layout);

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

void Settings::on_view_colouredBG_changed(int state)
{
	GlobalSettings settings;
	settings.setValue(GlobalSettings::Key_View_ColouredBG, state ? true : false);
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

void Settings::on_view_showAnalogMinorGrid_changed(int state)
{
	GlobalSettings settings;
	settings.setValue(GlobalSettings::Key_View_ShowAnalogMinorGrid, state ? true : false);
}

void Settings::on_view_defaultDivHeight_changed(int value)
{
	GlobalSettings settings;
	settings.setValue(GlobalSettings::Key_View_DefaultDivHeight, value);
}

void Settings::on_dec_initialStateConfigurable_changed(int state)
{
	GlobalSettings settings;
	settings.setValue(GlobalSettings::Key_Dec_InitialStateConfigurable, state ? true : false);
}

} // namespace dialogs
} // namespace pv
