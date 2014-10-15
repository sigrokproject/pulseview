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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifdef ENABLE_DECODE
#include <libsigrokdecode/libsigrokdecode.h> /* First, so we avoid a _POSIX_C_SOURCE warning. */
#endif

#include <stdint.h>
#include <libsigrok/libsigrok.hpp>

#include <getopt.h>

#include <QDebug>

#ifdef ENABLE_SIGNALS
#include "signalhandler.h"
#endif

#include "pv/application.h"
#include "pv/devicemanager.h"
#include "pv/mainwindow.h"
#ifdef ANDROID
#include <libsigrokandroidutils/libsigrokandroidutils.h>
#include "android/loghandler.h"
#endif

#include "config.h"

#ifdef _WIN32
// The static qsvg lib is required for SVG graphics/icons (on Windows).
#include <QtPlugin>
Q_IMPORT_PLUGIN(qsvg)
#endif

void usage()
{
	fprintf(stdout,
		"Usage:\n"
		"  %s [OPTION…] [FILE] — %s\n"
		"\n"
		"Help Options:\n"
		"  -l, --loglevel                  Set libsigrok/libsigrokdecode loglevel\n"
		"  -V, --version                   Show release version\n"
		"  -h, -?, --help                  Show help option\n"
		"\n", PV_BIN_NAME, PV_DESCRIPTION);
}

int main(int argc, char *argv[])
{
	int ret = 0;
	std::shared_ptr<sigrok::Context> context;
	const char *open_file = NULL;

	Application a(argc, argv);

#ifdef ANDROID
	srau_init_environment();
	pv::AndroidLogHandler::install_callbacks();
#endif

	// Parse arguments
	while (1) {
		static const struct option long_options[] = {
			{"loglevel", required_argument, 0, 'l'},
			{"version", no_argument, 0, 'V'},
			{"help", no_argument, 0, 'h'},
			{0, 0, 0, 0}
		};

		const int c = getopt_long(argc, argv,
			"l:Vh?", long_options, NULL);
		if (c == -1)
			break;

		switch (c) {
		case 'l':
		{
			const int loglevel = atoi(optarg);
			context->set_log_level(sigrok::LogLevel::get(loglevel));

#ifdef ENABLE_DECODE
			srd_log_loglevel_set(loglevel);
#endif

			break;
		}

		case 'V':
			// Print version info
			fprintf(stdout, "%s %s\n", PV_TITLE, PV_VERSION_STRING);
			return 0;

		case 'h':
		case '?':
			usage();
			return 0;
		}
	}

	if (argc - optind > 1) {
		fprintf(stderr, "Only one file can be openened.\n");
		return 1;
	} else if (argc - optind == 1)
		open_file = argv[argc - 1];

	// Initialise libsigrok
	context = sigrok::Context::create();

	do {

#ifdef ENABLE_DECODE
		// Initialise libsigrokdecode
		if (srd_init(NULL) != SRD_OK) {
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
			pv::MainWindow w(device_manager, open_file);
			w.show();

#ifdef ENABLE_SIGNALS
			if(SignalHandler::prepare_signals()) {
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

		} catch(std::exception e) {
			qDebug() << e.what();
		}

#ifdef ENABLE_DECODE
		// Destroy libsigrokdecode
		srd_exit();
#endif

	} while (0);

	return ret;
}
