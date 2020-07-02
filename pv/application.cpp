/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2014 Martin Ling <martin-sigrok@earth.li>
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

#include <iostream>
#include <typeinfo>

#include <QDebug>
#include <QDir>
#include <QLibraryInfo>
#include <QMessageBox>
#include <QWidget>

#include <boost/version.hpp>

#ifdef ENABLE_STACKTRACE
#include <boost/stacktrace.hpp>
#endif

#ifdef ENABLE_DECODE
#include <libsigrokdecode/libsigrokdecode.h>
#endif

#include "application.hpp"
#include "config.h"
#include "globalsettings.hpp"

using std::cout;
using std::endl;
using std::exception;
using std::shared_ptr;

#ifdef ENABLE_DECODE
static gint sort_pds(gconstpointer a, gconstpointer b)
{
	const struct srd_decoder *sda, *sdb;

	sda = (const struct srd_decoder *)a;
	sdb = (const struct srd_decoder *)b;
	return strcmp(sda->id, sdb->id);
}
#endif

Application::Application(int &argc, char* argv[]) :
	QApplication(argc, argv)
{
	setApplicationVersion(PV_VERSION_STRING);
	setApplicationName("PulseView");
	setOrganizationName("sigrok");
	setOrganizationDomain("sigrok.org");
}

const QStringList Application::get_languages() const
{
	QStringList files = QDir(":/l10n/").entryList(QStringList("*.qm"), QDir::Files);

	QStringList result;
	result << "en";  // Add default language to the set

	// Remove file extensions
	for (const QString& file : files)
		result << file.split(".").front();

	result.sort(Qt::CaseInsensitive);

	return result;
}

const QString Application::get_language_editors(const QString& language) const
{
	if (language == "de") return "Sören Apel, Uwe Hermann";
	if (language == "es_mx") return "Carlos Diaz";

	return QString();
}

void Application::switch_language(const QString& language)
{
	removeTranslator(&app_translator_);
	removeTranslator(&qt_translator_);
	removeTranslator(&qtbase_translator_);

	if ((language != "C") && (language != "en")) {
		// Application translations
		QString resource = ":/l10n/" + language +".qm";
		if (app_translator_.load(resource))
			installTranslator(&app_translator_);
		else
			qWarning() << "Translation resource" << resource << "not found";

		// Qt translations
		QString tr_path(QLibraryInfo::location(QLibraryInfo::TranslationsPath));

		if (qt_translator_.load("qt_" + language, tr_path))
			installTranslator(&qt_translator_);
		else
			qWarning() << "QT translations for" << language << "not found at" <<
				tr_path << ", Qt translations package is probably missing";

		// Qt base translations
		if (qtbase_translator_.load("qtbase_" + language, tr_path))
			installTranslator(&qtbase_translator_);
		else
			qWarning() << "QT base translations for" << language << "not found at" <<
				tr_path << ", Qt translations package is probably missing";
	}

	if (!topLevelWidgets().empty()) {
		// Force all windows to update
		for (QWidget *widget : topLevelWidgets())
			widget->update();

		QMessageBox msg(topLevelWidgets().front());
		msg.setText(tr("Some parts of the application may still " \
				"use the previous language. Re-opening the affected windows or " \
				"restarting the application will remedy this."));
		msg.setStandardButtons(QMessageBox::Ok);
		msg.setIcon(QMessageBox::Information);
		msg.exec();
	}
}

void Application::on_setting_changed(const QString &key, const QVariant &value)
{
	if (key == pv::GlobalSettings::Key_General_Language)
		switch_language(value.toString());
}

void Application::collect_version_info(shared_ptr<sigrok::Context> context)
{
	// Library versions and features
	version_info_.emplace_back(applicationName(), applicationVersion());
	version_info_.emplace_back("Qt", qVersion());
	version_info_.emplace_back("glibmm", PV_GLIBMM_VERSION);
	version_info_.emplace_back("Boost", BOOST_LIB_VERSION);

	version_info_.emplace_back("libsigrok", QString("%1/%2 (rt: %3/%4)")
		.arg(SR_PACKAGE_VERSION_STRING, SR_LIB_VERSION_STRING,
		sr_package_version_string_get(), sr_lib_version_string_get()));

	GSList *l_orig = sr_buildinfo_libs_get();
	for (GSList *l = l_orig; l; l = l->next) {
		GSList *m = (GSList *)l->data;
		const char *lib = (const char *)m->data;
		const char *version = (const char *)m->next->data;
		version_info_.emplace_back(QString(" - %1").arg(QString(lib)), QString(version));
		g_slist_free_full(m, g_free);
	}
	g_slist_free(l_orig);

	char *host = sr_buildinfo_host_get();
	version_info_.emplace_back(" - Host", QString(host));
	g_free(host);

	char *scpi_backends = sr_buildinfo_scpi_backends_get();
	version_info_.emplace_back(" - SCPI backends", QString(scpi_backends));
	g_free(scpi_backends);

#ifdef ENABLE_DECODE
	struct srd_decoder *dec;

	version_info_.emplace_back("libsigrokdecode", QString("%1/%2 (rt: %3/%4)")
		.arg(SRD_PACKAGE_VERSION_STRING, SRD_LIB_VERSION_STRING,
		srd_package_version_string_get(), srd_lib_version_string_get()));

	l_orig = srd_buildinfo_libs_get();
	for (GSList *l = l_orig; l; l = l->next) {
		GSList *m = (GSList *)l->data;
		const char *lib = (const char *)m->data;
		const char *version = (const char *)m->next->data;
		version_info_.emplace_back(QString(" - %1").arg(QString(lib)), QString(version));
		g_slist_free_full(m, g_free);
	}
	g_slist_free(l_orig);

	host = srd_buildinfo_host_get();
	version_info_.emplace_back(" - Host", QString(host));
	g_free(host);
#endif

	// Firmware paths
	l_orig = sr_resourcepaths_get(SR_RESOURCE_FIRMWARE);
	for (GSList *l = l_orig; l; l = l->next)
		fw_path_list_.emplace_back((char*)l->data);
	g_slist_free_full(l_orig, g_free);

	// PD paths
#ifdef ENABLE_DECODE
	l_orig = srd_searchpaths_get();
	for (GSList *l = l_orig; l; l = l->next)
		pd_path_list_.emplace_back((char*)l->data);
	g_slist_free_full(l_orig, g_free);
#endif

	// Device drivers
	for (auto& entry : context->drivers())
		driver_list_.emplace_back(QString::fromUtf8(entry.first.c_str()),
			QString::fromUtf8(entry.second->long_name().c_str()));

	// Input formats
	for (auto& entry : context->input_formats())
		input_format_list_.emplace_back(QString::fromUtf8(entry.first.c_str()),
			QString::fromUtf8(entry.second->description().c_str()));

	// Output formats
	for (auto& entry : context->output_formats())
		output_format_list_.emplace_back(QString::fromUtf8(entry.first.c_str()),
			QString::fromUtf8(entry.second->description().c_str()));

	// Protocol decoders
#ifdef ENABLE_DECODE
	GSList *sl = g_slist_copy((GSList *)srd_decoder_list());
	sl = g_slist_sort(sl, sort_pds);
	for (const GSList *l = sl; l; l = l->next) {
		dec = (struct srd_decoder *)l->data;
		pd_list_.emplace_back(QString::fromUtf8(dec->id),
			QString::fromUtf8(dec->longname));
	}
	g_slist_free(sl);
#endif
}

void Application::print_version_info()
{
	cout << PV_TITLE << " " << PV_VERSION_STRING << endl;

	cout << endl << "Libraries and features:" << endl;
	for (pair<QString, QString>& entry : version_info_)
		cout << "  " << entry.first.toStdString() << " " << entry.second.toStdString() << endl;

	cout << endl << "Firmware search paths:" << endl;
	for (QString& entry : fw_path_list_)
		cout << "  " << entry.toStdString() << endl;

	cout << endl << "Protocol decoder search paths:" << endl;
	for (QString& entry : pd_path_list_)
		cout << "  " << entry.toStdString() << endl;

	cout << endl << "Supported hardware drivers:" << endl;
	for (pair<QString, QString>& entry : driver_list_)
		cout << "  " << entry.first.leftJustified(21, ' ').toStdString() <<
		entry.second.toStdString() << endl;

	cout << endl << "Supported input formats:" << endl;
	for (pair<QString, QString>& entry : input_format_list_)
		cout << "  " << entry.first.leftJustified(21, ' ').toStdString() <<
		entry.second.toStdString() << endl;

	cout << endl << "Supported output formats:" << endl;
	for (pair<QString, QString>& entry : output_format_list_)
		cout << "  " << entry.first.leftJustified(21, ' ').toStdString() <<
		entry.second.toStdString() << endl;

#ifdef ENABLE_DECODE
	cout << endl << "Supported protocol decoders:" << endl;
	for (pair<QString, QString>& entry : pd_list_)
		cout << "  " << entry.first.leftJustified(21, ' ').toStdString() <<
		entry.second.toStdString() << endl;
#endif
}

vector< pair<QString, QString> > Application::get_version_info() const
{
	return version_info_;
}

vector<QString> Application::get_fw_path_list() const
{
	return fw_path_list_;
}

vector<QString> Application::get_pd_path_list() const
{
	return pd_path_list_;
}

vector< pair<QString, QString> > Application::get_driver_list() const
{
	return driver_list_;
}

vector< pair<QString, QString> > Application::get_input_format_list() const
{
	return input_format_list_;
}

vector< pair<QString, QString> > Application::get_output_format_list() const
{
	return output_format_list_;
}

vector< pair<QString, QString> > Application::get_pd_list() const
{
	return pd_list_;
}

bool Application::notify(QObject *receiver, QEvent *event)
{
	try {
		return QApplication::notify(receiver, event);
	} catch (exception& e) {
		qDebug().nospace() << "Caught exception of type " << \
			typeid(e).name() << " (" << e.what() << ")";
#ifdef ENABLE_STACKTRACE
		throw e;
#else
		exit(1);
#endif
		return false;
	}
}
