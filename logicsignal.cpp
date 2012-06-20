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

#include <math.h>

#include "logicdata.h"
#include "logicdatasnapshot.h"
#include "logicsignal.h"

using namespace boost;
using namespace std;

const float Log2 = logf(2.0f);

LogicSignal::LogicSignal(QString name, shared_ptr<LogicData> data,
	int probe_index) :
	Signal(name),
	_data(data),
	_probe_index(probe_index)
{
	assert(_probe_index >= 0);
}

void LogicSignal::paint(QGLWidget &widget, const QRect &rect,
	double scale, double offset)
{
	Point2F *vertex;

	vector< pair<int64_t, bool> > edges;

	assert(scale > 0);
	assert(_data);

	const queue< shared_ptr<LogicDataSnapshot> > &snapshots =
		_data->get_snapshots();
	if(snapshots.empty())
		return;

	const shared_ptr<LogicDataSnapshot> &snapshot = snapshots.front();

	const double pixels_offset = offset / scale;
	const double samplerate = _data->get_samplerate();
	const double start_time = _data->get_start_time();
	const int64_t last_sample = (int64_t)snapshot->get_sample_count() - 1;
	const double samples_per_pixel = samplerate * scale;
	const int64_t start = min(max((int64_t)(samplerate * (offset - start_time)),
		(int64_t)0), last_sample);
	const int64_t end = min((int64_t)(start + samples_per_pixel * rect.width()),
		last_sample);
	const int64_t quantization_length = 1LL << (int64_t)floorf(
		max(logf(samples_per_pixel / Log2), 0.0f));

	snapshot->get_subsampled_edges(edges, start, end,
		quantization_length, _probe_index);

	// Paint the edges
	const unsigned int edge_point_count = (edges.size() - 2) * 2;
	Point2F *const edge_points = new Point2F[edge_point_count];
	vertex = edge_points;

	for(vector<LogicDataSnapshot::EdgePair>::const_iterator i = edges.begin() + 1;
	    i != edges.end() - 1; i++)
	{
		const int x = (int)((*i).first / samples_per_pixel - pixels_offset) +
			rect.left();

		vertex->x = x, vertex->y = 10 + rect.top() - 1;
		vertex++;
		vertex->x = x, vertex->y = 40 + rect.top();
		vertex++;
	}

	glColor3f(0,0,1);
	paint_lines(edge_points, edge_point_count);
	delete[] edge_points;

	// Paint the caps
	const unsigned int cap_point_count = (edges.size() - 1) * 2;
	Point2F *const cap_points = new Point2F[cap_point_count];
	vertex = cap_points;

	for(vector<LogicDataSnapshot::EdgePair>::const_iterator i = edges.begin();
	    i != (edges.end() - 1); i++)
	{
		const int y = ((*i).second ? 10 : 40) + rect.top();

		vertex->x = (int)((*i).first / samples_per_pixel - pixels_offset) +
			rect.left() - 1;
		vertex->y = y;
		vertex++;

		vertex->x = (int)((*(i+1)).first / samples_per_pixel - pixels_offset) +
			rect.left();
		vertex->y = y;
		vertex++;
	}

	glColor3f(0,0,1);
	paint_lines(cap_points, cap_point_count);
	delete[] cap_points;
}

void LogicSignal::paint_lines(Point2F *points, int count)
{
	GLuint vbo_id;

	assert(points);

	glGenBuffers(1, &vbo_id);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_id);

	const unsigned int vbo_length = count * sizeof(Point2F);
	glBufferData(GL_ARRAY_BUFFER, vbo_length, NULL, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, vbo_length, points);

	glBindBuffer(GL_ARRAY_BUFFER, vbo_id);

	glVertexPointer(2, GL_FLOAT, sizeof(Point2F), 0);

	glEnableClientState(GL_VERTEX_ARRAY);
	glDrawArrays(GL_LINES,  0,  count);
	glDisableClientState(GL_VERTEX_ARRAY);

	glDeleteBuffers(1, &vbo_id);
}
