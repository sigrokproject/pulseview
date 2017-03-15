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

#include <stdint.h>
#include <libsigrokcxx/libsigrokcxx.hpp>

#include <getopt.h>

#include <QDebug>

#ifdef ENABLE_SIGNALS
#include "signalhandler.hpp"
#endif

#include "pv/application.hpp"
#include "pv/devicemanager.hpp"
#include "pv/mainwindow.hpp"
#ifdef ANDROID
#include <libsigrokandroidutils/libsigrokandroidutils.h>
#include "android/assetreader.hpp"
#include "android/loghandler.hpp"
#endif

#include "config.h"

#ifdef _WIN32
#include <QtPlugin>
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
Q_IMPORT_PLUGIN(QSvgPlugin)
#endif

using std::exception;
using std::shared_ptr;
using std::string;

void usage()
{
	fprintf(stdout,
		"Usage:\n"
		"  %s [OPTION…] — %s\n"
		"\n"
		"Help Options:\n"
		"  -h, -?, --help                  Show help option\n"
		"\n"
		"Application Options:\n"
		"  -V, --version                   Show release version\n"
		"  -l, --loglevel                  Set libsigrok/libsigrokdecode loglevel\n"
		"  -i, --input-file                Load input from file\n"
		"  -I, --input-format              Input format\n"
		"\n", PV_BIN_NAME, PV_DESCRIPTION);
}

int main(int argc, char *argv[])
{
	int ret = 0;
	shared_ptr<sigrok::Context> context;
	string open_file, open_file_format;

	Application a(argc, argv);

#ifdef ANDROID
	srau_init_environment();
	pv::AndroidLogHandler::install_callbacks();
	pv::AndroidAssetReader asset_reader;
#endif

	// Parse arguments
	while (1) {
		static const struct option long_options[] = {
			{"help", no_argument, nullptr, 'h'},
			{"version", no_argument, nullptr, 'V'},
			{"loglevel", required_argument, nullptr, 'l'},
			{"input-file", required_argument, nullptr, 'i'},
			{"input-format", required_argument, nullptr, 'I'},
			{nullptr, 0, nullptr, 0}
		};

		const int c = getopt_long(argc, argv,
			"l:Vh?i:I:", long_options, nullptr);
		if (c == -1)
			break;

		switch (c) {
		case 'h':
		case '?':
			usage();
			return 0;

		case 'V':
			// Print version info
			fprintf(stdout, "%s %s\n", PV_TITLE, PV_VERSION_STRING);
			return 0;

		case 'l':
		{
			const int loglevel = atoi(optarg);
			context->set_log_level(sigrok::LogLevel::get(loglevel));

#ifdef ENABLE_DECODE
			srd_log_loglevel_set(loglevel);
#endif

			break;
		}

		case 'i':
			open_file = optarg;
			break;

		case 'I':
			open_file_format = optarg;
			break;
		}
	}

	if (argc - optind > 1) {
		fprintf(stderr, "Only one file can be opened.\n");
		return 1;
	} else if (argc - optind == 1) {
		open_file = argv[argc - 1];
	}

	// Initialise libsigrok
	context = sigrok::Context::create();
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

		try {
			// Create the device manager, initialise the drivers
			pv::DeviceManager device_manager(context);

			// Initialise the main window
			pv::MainWindow w(device_manager,
				open_file, open_file_format);
			w.show();

#ifdef ENABLE_SIGNALS
			if (SignalHandler::prepare_signals()) {
				SignalHandler *const handler =
					new SignalHandler(&w);
				QObject::connect(handler,
					SIGNAL(int_received()),
					&w, SLOT(close()));
				QObject::connect(handler,
					SIGNAL(term_received()),
					&w, SLOT(close()));
			} else {
				qWarning() <<
					"Could not prepare signal handler.";
			}
#endif

			// Run the application
			ret = a.exec();

		} catch (exception e) {
			qDebug() << e.what();
		}

#ifdef ENABLE_DECODE
		// Destroy libsigrokdecode
		srd_exit();
#endif

	} while (0);

	return ret;
}
