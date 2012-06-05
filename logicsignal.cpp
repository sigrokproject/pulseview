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

#include "logicsignal.h"

LogicSignal::LogicSignal(QString name, boost::shared_ptr<SignalData> data,
	int probe_index) :
	Signal(name, data),
	_probe_index(probe_index)
{
	assert(_probe_index >= 0);
}

void LogicSignal::paint(QGLWidget &widget, const QRect &rect)
{
	glColor3f(1,0,0);
	glBegin(GL_POLYGON);
	glVertex2f(rect.left(), rect.top());
	glVertex2f(rect.right(), rect.top());
	glVertex2f(rect.right(), rect.bottom());
	glEnd();
}
