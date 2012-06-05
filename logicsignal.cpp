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

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>

#include "logicsignal.h"

struct LogicVertex
{
	GLfloat x, y;
};

LogicSignal::LogicSignal(QString name, boost::shared_ptr<SignalData> data,
	int probe_index) :
	Signal(name, data),
	_probe_index(probe_index)
{
	assert(_probe_index >= 0);
}

void LogicSignal::paint(QGLWidget &widget, const QRect &rect,
	uint64_t scale, int64_t offset)
{
	GLuint vbo_id;

	glColor3f(0,0,1);
	LogicVertex vetices[3];
	vetices[0].x = rect.left();
	vetices[0].y = rect.top();
	vetices[1].x = rect.right();
	vetices[1].y = rect.bottom();
	vetices[2].x = rect.right();
	vetices[2].y = rect.top();

	glGenBuffers(1, &vbo_id);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_id);

	glBufferData(GL_ARRAY_BUFFER, sizeof(vetices), NULL, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vetices), vetices);

	glBindBuffer(GL_ARRAY_BUFFER, vbo_id);

	glVertexPointer(2, GL_FLOAT, sizeof(LogicVertex), 0);

	glEnableClientState(GL_VERTEX_ARRAY);
	glDrawArrays(GL_LINE_STRIP,  0,  2);
	glDisableClientState(GL_VERTEX_ARRAY);
}
