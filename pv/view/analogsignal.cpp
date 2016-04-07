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

#include <extdef.h>

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <limits>

#include <QApplication>

#include "analogsignal.hpp"
#include "pv/data/analog.hpp"
#include "pv/data/analogsegment.hpp"
#include "pv/view/view.hpp"

#include <libsigrokcxx/libsigrokcxx.hpp>

using std::max;
using std::make_pair;
using std::min;
using std::shared_ptr;
using std::deque;

using sigrok::Channel;

namespace pv {
namespace view {

const QColor AnalogSignal::SignalColours[4] = {
	QColor(0xC4, 0xA0, 0x00),	// Yellow
	QColor(0x87, 0x20, 0x7A),	// Magenta
	QColor(0x20, 0x4A, 0x87),	// Blue
	QColor(0x4E, 0x9A, 0x06)	// Green
};

const QColor AnalogSignal::GridMajorColor = QColor(0xB0, 0xB0, 0xB0);
const QColor AnalogSignal::GridMinorColor = QColor(0xD0, 0xD0, 0xD0);

const float AnalogSignal::EnvelopeThreshold = 256.0f;

AnalogSignal::AnalogSignal(
	pv::Session &session,
	shared_ptr<Channel> channel,
	shared_ptr<data::Analog> data) :
	Signal(session, channel),
	data_(data),
	scale_index_(0),
	scale_index_drag_offset_(0),
	div_height_(3 * QFontMetrics(QApplication::font()).height()),
	vdivs_(1)
{
	set_colour(SignalColours[channel_->index() % countof(SignalColours)]);
}

shared_ptr<pv::data::SignalData> AnalogSignal::data() const
{
	return data_;
}

shared_ptr<pv::data::Analog> AnalogSignal::analog_data() const
{
	return data_;
}

std::pair<int, int> AnalogSignal::v_extents() const
{
	const int h = vdivs_ * div_height_;
	return make_pair(-h, h);
}

int AnalogSignal::scale_handle_offset() const
{
	const int h = vdivs_ * div_height_;

	return ((scale_index_drag_offset_ - scale_index_) *
		h / 4) - h / 2;
}

void AnalogSignal::scale_handle_dragged(int offset)
{
	const int h = vdivs_ * div_height_;

	scale_index_ = scale_index_drag_offset_ -
		(offset + h / 2) / (h / 4);
}

void AnalogSignal::scale_handle_drag_release()
{
	scale_index_drag_offset_ = scale_index_;
}

void AnalogSignal::paint_back(QPainter &p, const ViewItemPaintParams &pp)
{
	if (channel_->enabled()) {
		Trace::paint_back(p, pp);
		paint_axis(p, pp, get_visual_y());
	}
}

void AnalogSignal::paint_mid(QPainter &p, const ViewItemPaintParams &pp)
{
	assert(data_);
	assert(owner_);

	const int y = get_visual_y();

	if (!channel_->enabled())
		return;

	paint_grid(p, y, pp.left(), pp.right());

	const deque< shared_ptr<pv::data::AnalogSegment> > &segments =
		data_->analog_segments();
	if (segments.empty())
		return;

	const shared_ptr<pv::data::AnalogSegment> &segment =
		segments.front();

	const double pixels_offset = pp.pixels_offset();
	const double samplerate = max(1.0, segment->samplerate());
	const pv::util::Timestamp& start_time = segment->start_time();
	const int64_t last_sample = segment->get_sample_count() - 1;
	const double samples_per_pixel = samplerate * pp.scale();
	const pv::util::Timestamp start = samplerate * (pp.offset() - start_time);
	const pv::util::Timestamp end = start + samples_per_pixel * pp.width();

	const int64_t start_sample = min(max(floor(start).convert_to<int64_t>(),
		(int64_t)0), last_sample);
	const int64_t end_sample = min(max((ceil(end) + 1).convert_to<int64_t>(),
		(int64_t)0), last_sample);

	if (samples_per_pixel < EnvelopeThreshold)
		paint_trace(p, segment, y, pp.left(),
			start_sample, end_sample,
			pixels_offset, samples_per_pixel);
	else
		paint_envelope(p, segment, y, pp.left(),
			start_sample, end_sample,
			pixels_offset, samples_per_pixel);
}

void AnalogSignal::paint_grid(QPainter &p, int y, int left, int right)
{
	p.setPen(QPen(GridMajorColor, 0.5, Qt::DashLine));
	for (int i = 1; i <= vdivs_; i++) {
		const int dy = i * div_height_;
		p.drawLine(QLineF(left, y - dy, right, y - dy));
		p.drawLine(QLineF(left, y + dy, right, y + dy));
	}

	p.setPen(QPen(GridMinorColor, 0.5, Qt::DashLine));
	for (int i = 0; i < vdivs_; i++) {
		const int dy = i * div_height_;
		const float dy25 = dy + (0.25 * div_height_);
		const float dy50 = dy + (0.50 * div_height_);
		const float dy75 = dy + (0.75 * div_height_);
		p.drawLine(QLineF(left, y - dy25, right, y - dy25));
		p.drawLine(QLineF(left, y + dy25, right, y + dy25));
		p.drawLine(QLineF(left, y - dy50, right, y - dy50));
		p.drawLine(QLineF(left, y + dy50, right, y + dy50));
		p.drawLine(QLineF(left, y - dy75, right, y - dy75));
		p.drawLine(QLineF(left, y + dy75, right, y + dy75));
	}
}

void AnalogSignal::paint_trace(QPainter &p,
	const shared_ptr<pv::data::AnalogSegment> &segment,
	int y, int left, const int64_t start, const int64_t end,
	const double pixels_offset, const double samples_per_pixel)
{
	const float scale = this->scale();
	const int64_t sample_count = end - start;

	const float *const samples = segment->get_samples(start, end);
	assert(samples);

	p.setPen(colour_);

	QPointF *points = new QPointF[sample_count];
	QPointF *point = points;

	for (int64_t sample = start; sample != end; sample++) {
		const float x = (sample / samples_per_pixel -
			pixels_offset) + left;
		*point++ = QPointF(x,
			y - samples[sample - start] * scale);
	}

	p.drawPolyline(points, point - points);

	delete[] samples;
	delete[] points;
}

void AnalogSignal::paint_envelope(QPainter &p,
	const shared_ptr<pv::data::AnalogSegment> &segment,
	int y, int left, const int64_t start, const int64_t end,
	const double pixels_offset, const double samples_per_pixel)
{
	using pv::data::AnalogSegment;

	const float scale = this->scale();

	AnalogSegment::EnvelopeSection e;
	segment->get_envelope_section(e, start, end, samples_per_pixel);

	if (e.length < 2)
		return;

	p.setPen(QPen(Qt::NoPen));
	p.setBrush(colour_);

	QRectF *const rects = new QRectF[e.length];
	QRectF *rect = rects;

	for (uint64_t sample = 0; sample < e.length-1; sample++) {
		const float x = ((e.scale * sample + e.start) /
			samples_per_pixel - pixels_offset) + left;
		const AnalogSegment::EnvelopeSample *const s =
			e.samples + sample;

		// We overlap this sample with the next so that vertical
		// gaps do not appear during steep rising or falling edges
		const float b = y - max(s->max, (s+1)->min) * scale;
		const float t = y - min(s->min, (s+1)->max) * scale;

		float h = b - t;
		if (h >= 0.0f && h <= 1.0f)
			h = 1.0f;
		if (h <= 0.0f && h >= -1.0f)
			h = -1.0f;

		*rect++ = QRectF(x, t, 1.0f, h);
	}

	p.drawRects(rects, e.length);

	delete[] rects;
	delete[] e.samples;
}

float AnalogSignal::scale() const
{
	const float seq[] = {1.0f, 2.0f, 5.0f};
	const int offset = std::numeric_limits<int>::max() / (2 * countof(seq));
	const std::div_t d = std::div(
		(int)(scale_index_ + countof(seq) * offset),
		countof(seq));
	return powf(10.0f, d.quot - offset) * seq[d.rem];
}

} // namespace view
} // namespace pv
