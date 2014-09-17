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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "application.h"
#include "config.h"

#include <iostream>

Application::Application(int &argc, char* argv[]) :
	QApplication(argc, argv)
{
	setApplicationVersion(PV_VERSION_STRING);
	setApplicationName("PulseView");
	setOrganizationName("sigrok");
	setOrganizationDomain("sigrok.org");
}

bool Application::notify(QObject *receiver, QEvent *event)
{
	try {
		return QApplication::notify(receiver, event);
	} catch (std::exception& e) {
		std::cerr << "Caught exception: " << e.what() << std::endl;
		exit(1);
		return false;
	}
}
