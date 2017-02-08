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

#include <extdef.h>

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <limits>

#include <QApplication>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QSpinBox>
#include <QString>

#include "analogsignal.hpp"
#include "pv/data/analog.hpp"
#include "pv/data/analogsegment.hpp"
#include "pv/data/signalbase.hpp"
#include "pv/view/view.hpp"

#include <libsigrokcxx/libsigrokcxx.hpp>

using std::max;
using std::make_pair;
using std::min;
using std::shared_ptr;
using std::deque;

namespace pv {
namespace views {
namespace TraceView {

const QColor AnalogSignal::SignalColours[4] = {
	QColor(0xC4, 0xA0, 0x00),	// Yellow
	QColor(0x87, 0x20, 0x7A),	// Magenta
	QColor(0x20, 0x4A, 0x87),	// Blue
	QColor(0x4E, 0x9A, 0x06)	// Green
};

const QColor AnalogSignal::GridMajorColor = QColor(0, 0, 0, 40*256/100);
const QColor AnalogSignal::GridMinorColor = QColor(0, 0, 0, 20*256/100);

const float AnalogSignal::EnvelopeThreshold = 256.0f;

const int AnalogSignal::MaximumVDivs = 10;
const int AnalogSignal::MinScaleIndex = -6;
const int AnalogSignal::MaxScaleIndex = 7;

const int AnalogSignal::InfoTextMarginRight = 20;
const int AnalogSignal::InfoTextMarginBottom = 5;

AnalogSignal::AnalogSignal(
	pv::Session &session,
	shared_ptr<data::SignalBase> base) :
	Signal(session, base),
	scale_index_(4), // 20 per div
	scale_index_drag_offset_(0),
	div_height_(3 * QFontMetrics(QApplication::font()).height()),
	vdivs_(1),
	resolution_(0)
{
	base_->set_colour(SignalColours[base_->index() % countof(SignalColours)]);
	update_scale();
}

shared_ptr<pv::data::SignalData> AnalogSignal::data() const
{
	return base_->analog_data();
}

void AnalogSignal::save_settings(QSettings &settings) const
{
	settings.setValue("vdivs", vdivs_);
	settings.setValue("scale_index", scale_index_);
}

void AnalogSignal::restore_settings(QSettings &settings)
{
	if (settings.contains("vdivs"))
		vdivs_ = settings.value("vdivs").toInt();

	if (settings.contains("scale_index")) {
		scale_index_ = settings.value("scale_index").toInt();
		update_scale();
	}
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

	update_scale();
}

void AnalogSignal::scale_handle_drag_release()
{
	scale_index_drag_offset_ = scale_index_;
	update_scale();
}

void AnalogSignal::paint_back(QPainter &p, const ViewItemPaintParams &pp)
{
	if (base_->enabled()) {
		Trace::paint_back(p, pp);
		paint_axis(p, pp, get_visual_y());
	}
}

void AnalogSignal::paint_mid(QPainter &p, const ViewItemPaintParams &pp)
{
	assert(base_->analog_data());
	assert(owner_);

	const int y = get_visual_y();

	if (!base_->enabled())
		return;

	paint_grid(p, y, pp.left(), pp.right());

	const deque< shared_ptr<pv::data::AnalogSegment> > &segments =
		base_->analog_data()->analog_segments();
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

void AnalogSignal::paint_fore(QPainter &p, const ViewItemPaintParams &pp)
{
	if (!enabled())
		return;

	const int y = get_visual_y();

	// Show the info section on the right side of the trace
	const QString infotext = QString("%1 V/div").arg(resolution_);

	p.setPen(base_->colour());
	p.setFont(QApplication::font());

	const QRectF bounding_rect = QRectF(pp.left(),
			y + v_extents().first,
			pp.width() - InfoTextMarginRight,
			v_extents().second - v_extents().first - InfoTextMarginBottom);

	p.drawText(bounding_rect, Qt::AlignRight | Qt::AlignBottom, infotext);
}

void AnalogSignal::paint_grid(QPainter &p, int y, int left, int right)
{
	p.setRenderHint(QPainter::Antialiasing, false);

	p.setPen(QPen(GridMajorColor, 1, Qt::DashLine));
	for (int i = 1; i <= vdivs_; i++) {
		const float dy = i * div_height_;
		p.drawLine(QLineF(left, y - dy, right, y - dy));
		p.drawLine(QLineF(left, y + dy, right, y + dy));
	}

	p.setPen(QPen(GridMinorColor, 1, Qt::DashLine));
	for (int i = 0; i < vdivs_; i++) {
		const float dy = i * div_height_;
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

	p.setRenderHint(QPainter::Antialiasing, true);
}

void AnalogSignal::paint_trace(QPainter &p,
	const shared_ptr<pv::data::AnalogSegment> &segment,
	int y, int left, const int64_t start, const int64_t end,
	const double pixels_offset, const double samples_per_pixel)
{
	p.setPen(base_->colour());

	QPointF *points = new QPointF[end - start];
	QPointF *point = points;

	pv::data::SegmentAnalogDataIterator* it =
		segment->begin_sample_iteration(start);

	for (int64_t sample = start; sample != end; sample++) {
		const float x = (sample / samples_per_pixel -
			pixels_offset) + left;

		*point++ = QPointF(x, y - *((float*)it->value) * scale_);
		segment->continue_sample_iteration(it, 1);
	}
	segment->end_sample_iteration(it);

	p.drawPolyline(points, point - points);

	delete[] points;
}

void AnalogSignal::paint_envelope(QPainter &p,
	const shared_ptr<pv::data::AnalogSegment> &segment,
	int y, int left, const int64_t start, const int64_t end,
	const double pixels_offset, const double samples_per_pixel)
{
	using pv::data::AnalogSegment;

	AnalogSegment::EnvelopeSection e;
	segment->get_envelope_section(e, start, end, samples_per_pixel);

	if (e.length < 2)
		return;

	p.setPen(QPen(Qt::NoPen));
	p.setBrush(base_->colour());

	QRectF *const rects = new QRectF[e.length];
	QRectF *rect = rects;

	for (uint64_t sample = 0; sample < e.length-1; sample++) {
		const float x = ((e.scale * sample + e.start) /
			samples_per_pixel - pixels_offset) + left;
		const AnalogSegment::EnvelopeSample *const s =
			e.samples + sample;

		// We overlap this sample with the next so that vertical
		// gaps do not appear during steep rising or falling edges
		const float b = y - max(s->max, (s+1)->min) * scale_;
		const float t = y - min(s->min, (s+1)->max) * scale_;

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

float AnalogSignal::get_resolution(int scale_index)
{
	const float seq[] = {1.0f, 2.0f, 5.0f};

	const int offset = std::numeric_limits<int>::max() / (2 * countof(seq));
	const std::div_t d = std::div(
		(int)(scale_index + countof(seq) * offset),
		countof(seq));

	return powf(10.0f, d.quot - offset) * seq[d.rem];
}

void AnalogSignal::update_scale()
{
	resolution_ = get_resolution(scale_index_);
	scale_ = div_height_ / resolution_;
}

void AnalogSignal::populate_popup_form(QWidget *parent, QFormLayout *form)
{
	// Add the standard options
	Signal::populate_popup_form(parent, form);

	QFormLayout *const layout = new QFormLayout;

	// Add the number of vdivs
	QSpinBox *vdiv_sb = new QSpinBox(parent);
	vdiv_sb->setRange(1, MaximumVDivs);
	vdiv_sb->setValue(vdivs_);
	connect(vdiv_sb, SIGNAL(valueChanged(int)),
		this, SLOT(on_vdivs_changed(int)));
	layout->addRow(tr("Number of vertical divs"), vdiv_sb);

	// Add the vertical resolution
	resolution_cb_ = new QComboBox(parent);

	for (int i = MinScaleIndex; i < MaxScaleIndex; i++) {
		const QString label = QString("%1").arg(get_resolution(i));
		resolution_cb_->insertItem(0, label, QVariant(i));
	}

	const int cur_idx = resolution_cb_->findData(QVariant(scale_index_));
	resolution_cb_->setCurrentIndex(cur_idx);

	connect(resolution_cb_, SIGNAL(currentIndexChanged(int)),
		this, SLOT(on_resolution_changed(int)));

	QGridLayout *const vdiv_layout = new QGridLayout;
	QLabel *const vdiv_unit = new QLabel(tr("V/div"));
	vdiv_layout->addWidget(resolution_cb_, 0, 0);
	vdiv_layout->addWidget(vdiv_unit, 0, 1);

	layout->addRow(tr("Vertical resolution"), vdiv_layout);

	form->addRow(layout);
}

void AnalogSignal::on_vdivs_changed(int vdivs)
{
	vdivs_ = vdivs;

	if (owner_) {
		// Call order is important, otherwise the lazy event handler won't work
		owner_->extents_changed(false, true);
		owner_->row_item_appearance_changed(false, true);
	}
}

void AnalogSignal::on_resolution_changed(int index)
{
	scale_index_ = resolution_cb_->itemData(index).toInt();
	update_scale();

	if (owner_)
		owner_->row_item_appearance_changed(false, true);
}

} // namespace TraceView
} // namespace views
} // namespace pv
