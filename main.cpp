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

#include <sigrokdecode.h> /* First, so we avoid a _POSIX_C_SOURCE warning. */
#include <stdint.h>
#include <libsigrok/libsigrok.h>

#include <getopt.h>

#include <QtGui/QApplication>
#include <QDebug>

#include "signalhandler.h"
#include "pv/mainwindow.h"

#include "config.h"

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
	struct sr_context *sr_ctx = NULL;
	const char *open_file = NULL;

	QApplication a(argc, argv);

	// Set some application metadata
	QApplication::setApplicationVersion(PV_VERSION_STRING);
	QApplication::setApplicationName("PulseView");
	QApplication::setOrganizationDomain("http://www.sigrok.org");

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
			sr_log_loglevel_set(loglevel);
			srd_log_loglevel_set(loglevel);
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
	if (sr_init(&sr_ctx) != SR_OK) {
		qDebug() << "ERROR: libsigrok init failed.";
		return 1;
	}

	// Initialise libsigrokdecode
	if (srd_init(NULL) == SRD_OK) {

		// Load the protocol decoders
		srd_decoder_load_all();

		// Initialize all libsigrok drivers
		sr_dev_driver **const drivers = sr_driver_list();
		for (sr_dev_driver **driver = drivers; *driver; driver++) {
			if (sr_driver_init(sr_ctx, *driver) != SR_OK) {
				qDebug("Failed to initialize driver %s",
					(*driver)->name);
				ret = 1;
				break;
			}
		}

		if (ret == 0) {
			// Initialise the main window
			pv::MainWindow w(open_file);
			w.show();

			if(SignalHandler::prepareSignals()) {
				SignalHandler *const signalHandler =
					new SignalHandler(&w);
				QObject::connect(signalHandler,
					SIGNAL(sigIntReceived()),
					&w, SLOT(close()));
				QObject::connect(signalHandler,
					SIGNAL(sigTermReceived()),
					&w, SLOT(close()));
    			} else {
				qWarning() <<
					"Could not prepare signal handler.";
			}

			// Run the application
			ret = a.exec();
		}

		// Destroy libsigrokdecode and libsigrok
		srd_exit();

	} else {
		qDebug() << "ERROR: libsigrokdecode init failed.";
	}

	if (sr_ctx)
		sr_exit(sr_ctx);

	return ret;
}
