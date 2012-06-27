/*
 * This file is part of the sigrok project.
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

extern "C" {
#include <sigrokdecode.h> /* First, so we avoid a _POSIX_C_SOURCE warning. */
#include <stdint.h>
#include <libsigrok/libsigrok.h>
}

#include <QtGui/QApplication>
#include <QDebug>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);

	/* Set some application metadata. */
	QApplication::setApplicationVersion(APP_VERSION);
	QApplication::setApplicationName("sigrok-qt");
	QApplication::setOrganizationDomain("http://www.sigrok.org");

	/* Initialise libsigrok */
	if (sr_init() != SR_OK) {
		qDebug() << "ERROR: libsigrok init failed.";
		return 1;
	}

	/* Initialise libsigrokdecode */
	if (srd_init(NULL) != SRD_OK) {
		qDebug() << "ERROR: libsigrokdecode init failed.";
		return 1;
	}

	/* Load the protocol decoders */
	srd_decoder_load_all();

	/* Initialize all libsigrok drivers. */
	sr_dev_driver **const drivers = sr_driver_list();
	for (sr_dev_driver **driver = drivers; *driver; driver++) {
		if (sr_driver_init(*driver) != SR_OK) {
			qDebug("Failed to initialize driver %s",
				(*driver)->name);
			return 1;
		}
	}

	/* Initialise the main window */
	MainWindow w;
	w.show();

	/* Run the application */
	const int ret = a.exec();

	/* Destroy libsigrokdecode and libsigrok */
	srd_exit();
	sr_exit();

	return ret;
}
