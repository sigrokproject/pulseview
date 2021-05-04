/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#ifdef ENABLE_DECODE
#include <libsigrokdecode/libsigrokdecode.h> /* First, so we avoid a _POSIX_C_SOURCE warning. */
#endif

#include <cstdint>
#include <fstream>
#include <getopt.h>
#include <vector>

#ifdef ENABLE_FLOW
#include <gstreamermm.h>
#include <libsigrokflow/libsigrokflow.hpp>
#endif

#include <libsigrokcxx/libsigrokcxx.hpp>

#include <QCheckBox>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QSettings>
#include <QTextStream>

#include "config.h"

#ifdef ENABLE_SIGNALS
#include "signalhandler.hpp"
#endif

#ifdef ENABLE_STACKTRACE
#include <signal.h>
#include <boost/stacktrace.hpp>
#include <QStandardPaths>
#endif

#include "pv/application.hpp"
#include "pv/devicemanager.hpp"
#include "pv/globalsettings.hpp"
#include "pv/logging.hpp"
#include "pv/mainwindow.hpp"
#include "pv/session.hpp"
#include "pv/util.hpp"
#include "pv/data/segment.hpp"

#ifdef ANDROID
#include <libsigrokandroidutils/libsigrokandroidutils.h>
#include "android/assetreader.hpp"
#include "android/loghandler.hpp"
#endif

#ifdef _WIN32
#include <QtPlugin>
#ifdef QT_STATIC
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
Q_IMPORT_PLUGIN(QSvgPlugin)
#endif
#endif

using std::exception;
using std::ifstream;
using std::ofstream;
using std::shared_ptr;
using std::string;

#if ENABLE_STACKTRACE
QString stacktrace_filename;

void signal_handler(int signum)
{
	::signal(signum, SIG_DFL);
	boost::stacktrace::safe_dump_to(stacktrace_filename.toLocal8Bit().data());
	::raise(SIGABRT);
}

void process_stacktrace(QString temp_path)
{
	const QString stacktrace_outfile = temp_path + "/pv_stacktrace.txt";

	ifstream ifs(stacktrace_filename.toLocal8Bit().data());
	ofstream ofs(stacktrace_outfile.toLocal8Bit().data(),
		ofstream::out | ofstream::trunc);

	boost::stacktrace::stacktrace st =
		boost::stacktrace::stacktrace::from_dump(ifs);
	ofs << st;

	ofs.close();
	ifs.close();

	QFile f(stacktrace_outfile);
	f.open(QFile::ReadOnly | QFile::Text);
	QTextStream fs(&f);
	QString stacktrace = fs.readAll();
	stacktrace = stacktrace.trimmed().replace('\n', "<br />");

	qDebug() << QObject::tr("Stack trace of previous crash:");
	qDebug() << "---------------------------------------------------------";
	// Note: qDebug() prints quotation marks for QString output, so we feed it char*
	qDebug() << stacktrace.toLocal8Bit().data();
	qDebug() << "---------------------------------------------------------";

	f.close();

	// Remove stack trace so we don't process it again the next time we run
	QFile::remove(stacktrace_filename.toLocal8Bit().data());

	// Show notification dialog if permitted
	pv::GlobalSettings settings;
	if (settings.value(pv::GlobalSettings::Key_Log_NotifyOfStacktrace).toBool()) {
		QCheckBox *cb = new QCheckBox(QObject::tr("Don't show this message again"));

		QMessageBox msgbox;
		msgbox.setText(QObject::tr("When %1 last crashed, it created a stack trace.\n" \
			"A human-readable form has been saved to disk and was written to " \
			"the log. You may access it from the settings dialog.").arg(PV_TITLE));
		msgbox.setIcon(QMessageBox::Icon::Information);
		msgbox.addButton(QMessageBox::Ok);
		msgbox.setCheckBox(cb);

		QObject::connect(cb, &QCheckBox::stateChanged, [](int state){
			pv::GlobalSettings settings;
			settings.setValue(pv::GlobalSettings::Key_Log_NotifyOfStacktrace,
				!state); });

		msgbox.exec();
	}
}
#endif

void usage()
{
	fprintf(stdout,
		"Usage:\n"
		"  %s [OPTIONS] [FILE]\n"
		"\n"
		"Help Options:\n"
		"  -h, -?, --help                  Show help option\n"
		"\n"
		"Application Options:\n"
		"  -V, --version                   Show release version\n"
		"  -l, --loglevel                  Set libsigrok/libsigrokdecode loglevel\n"
		"  -d, --driver                    Specify the device driver to use\n"
		"  -D, --dont-scan                 Don't auto-scan for devices, use -d spec only\n"
		"  -i, --input-file                Load input from file\n"
		"  -s, --settings                  Load PulseView session setup from file\n"
		"  -I, --input-format              Input format\n"
		"  -c, --clean                     Don't restore previous sessions on startup\n"
		"\n", PV_BIN_NAME);
}

int main(int argc, char *argv[])
{
	int ret = 0;
	shared_ptr<sigrok::Context> context;
	string open_file_format, open_setup_file, driver;
	vector<string> open_files;
	bool restore_sessions = true;
	bool do_scan = true;
	bool show_version = false;

#ifdef ENABLE_FLOW
	// Initialise gstreamermm. Must be called before any other GLib stuff.
	Gst::init();

	// Initialize libsigrokflow. Must be called after Gst::init().
	Srf::init();
#endif

	Application a(argc, argv);

#ifdef ANDROID
	srau_init_environment();
	pv::AndroidLogHandler::install_callbacks();
	pv::AndroidAssetReader asset_reader;
#endif

	// Parse arguments
	while (true) {
		static const struct option long_options[] = {
			{"help", no_argument, nullptr, 'h'},
			{"version", no_argument, nullptr, 'V'},
			{"loglevel", required_argument, nullptr, 'l'},
			{"driver", required_argument, nullptr, 'd'},
			{"dont-scan", no_argument, nullptr, 'D'},
			{"input-file", required_argument, nullptr, 'i'},
			{"settings", required_argument, nullptr, 's'},
			{"input-format", required_argument, nullptr, 'I'},
			{"clean", no_argument, nullptr, 'c'},
			{"log-to-stdout", no_argument, nullptr, 's'},
			{nullptr, 0, nullptr, 0}
		};

		const int c = getopt_long(argc, argv,
			"h?VDcl:d:i:s:I:", long_options, nullptr);
		if (c == -1)
			break;

		switch (c) {
		case 'h':
		case '?':
			usage();
			return 0;

		case 'V':
			show_version = true;
			break;

		case 'l':
		{
			const int loglevel = atoi(optarg);
			if (loglevel < 0 || loglevel > 5) {
				qDebug() << "ERROR: invalid log level spec.";
				break;
			}
			context->set_log_level(sigrok::LogLevel::get(loglevel));

#ifdef ENABLE_DECODE
			srd_log_loglevel_set(loglevel);
#endif

			if (loglevel >= 5) {
				const QSettings settings;
				qDebug() << "Settings:" << settings.fileName()
					<< "format" << settings.format();
			}
			break;
		}

		case 'd':
			driver = optarg;
			break;

		case 'D':
			do_scan = false;
			break;

		case 'i':
			open_files.emplace_back(optarg);
			break;

		case 's':
			open_setup_file = optarg;
			break;

		case 'I':
			open_file_format = optarg;
			break;

		case 'c':
			restore_sessions = false;
			break;
		}
	}
	argc -= optind;
	argv += optind;

	for (int i = 0; i < argc; i++)
		open_files.emplace_back(argv[i]);

	qRegisterMetaType<uint64_t>("uint64_t");
	qRegisterMetaType<pv::util::Timestamp>("util::Timestamp");
	qRegisterMetaType<SharedPtrToSegment>("SharedPtrToSegment");
	qRegisterMetaType<shared_ptr<pv::data::SignalBase>>("shared_ptr<SignalBase>");

	// Prepare the global settings since logging needs them early on
	pv::GlobalSettings settings;
	settings.add_change_handler(&a);  // Only the application object can't register itself
	settings.save_internal_defaults();
	settings.set_defaults_where_needed();
	settings.apply_language();
	settings.apply_theme();

	pv::logging.init();

	// Initialise libsigrok
	context = sigrok::Context::create();
	pv::Session::sr_context = context;

#if ENABLE_STACKTRACE
	QString temp_path = QStandardPaths::standardLocations(
		QStandardPaths::TempLocation).at(0);
	stacktrace_filename = temp_path + "/pv_stacktrace.dmp";
	qDebug() << "Stack trace file is" << stacktrace_filename;

	::signal(SIGSEGV, &signal_handler);
	::signal(SIGABRT, &signal_handler);

	if (QFileInfo::exists(stacktrace_filename))
		process_stacktrace(temp_path);
#endif

#ifdef ANDROID
	context->set_resource_reader(&asset_reader);
#endif
	do {

#ifdef ENABLE_DECODE
		// Initialise libsigrokdecode
		if (srd_init(nullptr) != SRD_OK) {
			qDebug() << "ERROR: libsigrokdecode init failed.";
			break;
		}

		// Load the protocol decoders
		srd_decoder_load_all();
#endif

#ifndef ENABLE_STACKTRACE
		try {
#endif

		// Create the device manager, initialise the drivers
		pv::DeviceManager device_manager(context, driver, do_scan);

		a.collect_version_info(context);
		if (show_version) {
			a.print_version_info();
		} else {
			// Initialise the main window
			pv::MainWindow w(device_manager);
			w.show();

			if (restore_sessions)
				w.restore_sessions();

			if (open_files.empty())
				w.add_default_session();
			else
				for (string& open_file : open_files)
					w.add_session_with_file(open_file, open_file_format, open_setup_file);

#ifdef ENABLE_SIGNALS
			if (SignalHandler::prepare_signals()) {
				SignalHandler *const handler = new SignalHandler(&w);
				QObject::connect(handler, SIGNAL(int_received()), &w, SLOT(close()));
				QObject::connect(handler, SIGNAL(term_received()), &w, SLOT(close()));
				QObject::connect(handler, SIGNAL(usr1_received()), &w, SLOT(on_run_stop_clicked()));
			} else
				qWarning() << "Could not prepare signal handler.";
#endif

			// Run the application
			ret = a.exec();
		}

#ifndef ENABLE_STACKTRACE
		} catch (exception& e) {
			qDebug() << "Exception:" << e.what();
		}
#endif

#ifdef ENABLE_DECODE
		// Destroy libsigrokdecode
		srd_exit();
#endif

	} while (false);

	return ret;
}
