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
#include <vector>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QString>

#include "analogsignal.hpp"
#include "pv/data/analog.hpp"
#include "pv/data/analogsegment.hpp"
#include "pv/data/logic.hpp"
#include "pv/data/logicsegment.hpp"
#include "pv/data/signalbase.hpp"
#include "pv/globalsettings.hpp"
#include "pv/view/logicsignal.hpp"
#include "pv/view/view.hpp"

#include <libsigrokcxx/libsigrokcxx.hpp>

using std::deque;
using std::div;
using std::div_t;
using std::max;
using std::make_pair;
using std::min;
using std::numeric_limits;
using std::pair;
using std::shared_ptr;
using std::vector;

namespace pv {
namespace views {
namespace TraceView {

const QColor AnalogSignal::SignalColours[4] = {
	QColor(0xC4, 0xA0, 0x00),	// Yellow
	QColor(0x87, 0x20, 0x7A),	// Magenta
	QColor(0x20, 0x4A, 0x87),	// Blue
	QColor(0x4E, 0x9A, 0x06)	// Green
};

const QColor AnalogSignal::GridMajorColor = QColor(0, 0, 0, 40 * 256 / 100);
const QColor AnalogSignal::GridMinorColor = QColor(0, 0, 0, 20 * 256 / 100);

const QColor AnalogSignal::SamplingPointColour(0x77, 0x77, 0x77);

const int64_t AnalogSignal::TracePaintBlockSize = 1024 * 1024;  // 4 MiB (due to float)
const float AnalogSignal::EnvelopeThreshold = 64.0f;

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
	pos_vdivs_(1),
	neg_vdivs_(1),
	resolution_(0),
	conversion_type_(data::SignalBase::NoConversion),
	display_type_(DisplayBoth),
	autoranging_(true)
{
	pv::data::Analog* analog_data =
		dynamic_cast<pv::data::Analog*>(data().get());

	connect(analog_data, SIGNAL(samples_added(QObject*, uint64_t, uint64_t)),
		this, SLOT(on_samples_added()));

	base_->set_colour(SignalColours[base_->index() % countof(SignalColours)]);
	update_scale();
}

shared_ptr<pv::data::SignalData> AnalogSignal::data() const
{
	return base_->analog_data();
}

void AnalogSignal::save_settings(QSettings &settings) const
{
	settings.setValue("pos_vdivs", pos_vdivs_);
	settings.setValue("neg_vdivs", neg_vdivs_);
	settings.setValue("scale_index", scale_index_);
	settings.setValue("conversion_type", conversion_type_);
	settings.setValue("display_type", display_type_);
	settings.setValue("autoranging", autoranging_);
}

void AnalogSignal::restore_settings(QSettings &settings)
{
	if (settings.contains("pos_vdivs"))
		pos_vdivs_ = settings.value("pos_vdivs").toInt();

	if (settings.contains("neg_vdivs"))
		neg_vdivs_ = settings.value("neg_vdivs").toInt();

	if (settings.contains("scale_index")) {
		scale_index_ = settings.value("scale_index").toInt();
		update_scale();
	}

	if (settings.contains("conversion_type")) {
		conversion_type_ = (data::SignalBase::ConversionType)(settings.value("conversion_type").toInt());
		update_conversion_type();
	}

	if (settings.contains("display_type"))
		display_type_ = (DisplayType)(settings.value("display_type").toInt());

	if (settings.contains("autoranging"))
		autoranging_ = settings.value("autoranging").toBool();
}

pair<int, int> AnalogSignal::v_extents() const
{
	const int ph = pos_vdivs_ * div_height_;
	const int nh = neg_vdivs_ * div_height_;
	return make_pair(-ph, nh);
}

int AnalogSignal::scale_handle_offset() const
{
	const int h = (pos_vdivs_ + neg_vdivs_) * div_height_;

	return ((scale_index_drag_offset_ - scale_index_) * h / 4) - h / 2;
}

void AnalogSignal::scale_handle_dragged(int offset)
{
	const int h = (pos_vdivs_ + neg_vdivs_) * div_height_;

	scale_index_ = scale_index_drag_offset_ - (offset + h / 2) / (h / 4);

	update_scale();
}

void AnalogSignal::scale_handle_drag_release()
{
	scale_index_drag_offset_ = scale_index_;
	update_scale();
}

void AnalogSignal::paint_back(QPainter &p, ViewItemPaintParams &pp)
{
	if (base_->enabled()) {
		Trace::paint_back(p, pp);
		paint_axis(p, pp, get_visual_y());
	}
}

void AnalogSignal::paint_mid(QPainter &p, ViewItemPaintParams &pp)
{
	assert(base_->analog_data());
	assert(owner_);

	const int y = get_visual_y();

	if (!base_->enabled())
		return;

	if ((display_type_ == DisplayAnalog) || (display_type_ == DisplayBoth)) {
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

	if ((display_type_ == DisplayConverted) || (display_type_ == DisplayBoth)) {
		if (((conversion_type_ == data::SignalBase::A2LConversionByTreshold) ||
			(conversion_type_ == data::SignalBase::A2LConversionBySchmittTrigger))) {

			paint_logic_mid(p, pp);
		}
	}
}

void AnalogSignal::paint_fore(QPainter &p, ViewItemPaintParams &pp)
{
	if (!enabled())
		return;

	if ((display_type_ == DisplayAnalog) || (display_type_ == DisplayBoth)) {
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
}

void AnalogSignal::paint_grid(QPainter &p, int y, int left, int right)
{
	p.setRenderHint(QPainter::Antialiasing, false);

	GlobalSettings settings;
	const bool show_analog_minor_grid =
		settings.value(GlobalSettings::Key_View_ShowAnalogMinorGrid).toBool();

	if (pos_vdivs_ > 0) {
		p.setPen(QPen(GridMajorColor, 1, Qt::DashLine));
		for (int i = 1; i <= pos_vdivs_; i++) {
			const float dy = i * div_height_;
			p.drawLine(QLineF(left, y - dy, right, y - dy));
		}
	}

	if ((pos_vdivs_ > 0) && show_analog_minor_grid) {
		p.setPen(QPen(GridMinorColor, 1, Qt::DashLine));
		for (int i = 0; i < pos_vdivs_; i++) {
			const float dy = i * div_height_;
			const float dy25 = dy + (0.25 * div_height_);
			const float dy50 = dy + (0.50 * div_height_);
			const float dy75 = dy + (0.75 * div_height_);
			p.drawLine(QLineF(left, y - dy25, right, y - dy25));
			p.drawLine(QLineF(left, y - dy50, right, y - dy50));
			p.drawLine(QLineF(left, y - dy75, right, y - dy75));
		}
	}

	if (neg_vdivs_ > 0) {
		p.setPen(QPen(GridMajorColor, 1, Qt::DashLine));
		for (int i = 1; i <= neg_vdivs_; i++) {
			const float dy = i * div_height_;
			p.drawLine(QLineF(left, y + dy, right, y + dy));
		}
	}

	if ((pos_vdivs_ > 0) && show_analog_minor_grid) {
		p.setPen(QPen(GridMinorColor, 1, Qt::DashLine));
		for (int i = 0; i < neg_vdivs_; i++) {
			const float dy = i * div_height_;
			const float dy25 = dy + (0.25 * div_height_);
			const float dy50 = dy + (0.50 * div_height_);
			const float dy75 = dy + (0.75 * div_height_);
			p.drawLine(QLineF(left, y + dy25, right, y + dy25));
			p.drawLine(QLineF(left, y + dy50, right, y + dy50));
			p.drawLine(QLineF(left, y + dy75, right, y + dy75));
		}
	}

	p.setRenderHint(QPainter::Antialiasing, true);
}

void AnalogSignal::paint_trace(QPainter &p,
	const shared_ptr<pv::data::AnalogSegment> &segment,
	int y, int left, const int64_t start, const int64_t end,
	const double pixels_offset, const double samples_per_pixel)
{
	if (end <= start)
		return;

	// Calculate and paint the sampling points if enabled and useful
	GlobalSettings settings;
	const bool show_sampling_points =
		settings.value(GlobalSettings::Key_View_ShowSamplingPoints).toBool() &&
		(samples_per_pixel < 0.25);

	p.setPen(base_->colour());

	const int64_t points_count = end - start;

	QPointF *points = new QPointF[points_count];
	QPointF *point = points;

	QRectF *sampling_points = nullptr;
	if (show_sampling_points)
		 sampling_points = new QRectF[points_count];
	QRectF *sampling_point = sampling_points;

	int64_t sample_count = min(points_count, TracePaintBlockSize);
	int64_t block_sample = 0;
	const float *sample_block = segment->get_samples(start, start + sample_count);

	const int w = 2;
	for (int64_t sample = start; sample != end; sample++, block_sample++) {

		if (block_sample == TracePaintBlockSize) {
			block_sample = 0;
			delete[] sample_block;
			sample_count = min(points_count - sample, TracePaintBlockSize);
			sample_block = segment->get_samples(sample, sample + sample_count);
		}

		const float x = (sample / samples_per_pixel -
			pixels_offset) + left;

		*point++ = QPointF(x, y - sample_block[block_sample] * scale_);

		if (show_sampling_points)
			*sampling_point++ =
				QRectF(x - (w / 2), y - sample_block[block_sample] * scale_ - (w / 2), w, w);
	}
	delete[] sample_block;

	p.drawPolyline(points, points_count);

	if (show_sampling_points) {
		p.setPen(SamplingPointColour);
		p.drawRects(sampling_points, points_count);
		delete[] sampling_points;
	}

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

	for (uint64_t sample = 0; sample < e.length - 1; sample++) {
		const float x = ((e.scale * sample + e.start) /
			samples_per_pixel - pixels_offset) + left;
		const AnalogSegment::EnvelopeSample *const s =
			e.samples + sample;

		// We overlap this sample with the next so that vertical
		// gaps do not appear during steep rising or falling edges
		const float b = y - max(s->max, (s + 1)->min) * scale_;
		const float t = y - min(s->min, (s + 1)->max) * scale_;

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

void AnalogSignal::paint_logic_mid(QPainter &p, ViewItemPaintParams &pp)
{
	QLineF *line;

	vector< pair<int64_t, bool> > edges;

	assert(base_);

	const int y = get_visual_y();

	if (!base_->enabled() || !base_->logic_data())
		return;

	const int signal_margin =
		QFontMetrics(QApplication::font()).height() / 2;

	const int ph = min(pos_vdivs_, 1) * div_height_;
	const int nh = min(neg_vdivs_, 1) * div_height_;
	const float high_offset = y - ph + signal_margin + 0.5f;
	const float low_offset = y + nh - signal_margin - 0.5f;

	const deque< shared_ptr<pv::data::LogicSegment> > &segments =
		base_->logic_data()->logic_segments();

	if (segments.empty())
		return;

	const shared_ptr<pv::data::LogicSegment> &segment =
		segments.front();

	double samplerate = segment->samplerate();

	// Show sample rate as 1Hz when it is unknown
	if (samplerate == 0.0)
		samplerate = 1.0;

	const double pixels_offset = pp.pixels_offset();
	const pv::util::Timestamp& start_time = segment->start_time();
	const int64_t last_sample = segment->get_sample_count() - 1;
	const double samples_per_pixel = samplerate * pp.scale();
	const double pixels_per_sample = 1 / samples_per_pixel;
	const pv::util::Timestamp start = samplerate * (pp.offset() - start_time);
	const pv::util::Timestamp end = start + samples_per_pixel * pp.width();

	const int64_t start_sample = min(max(floor(start).convert_to<int64_t>(),
		(int64_t)0), last_sample);
	const uint64_t end_sample = min(max(ceil(end).convert_to<int64_t>(),
		(int64_t)0), last_sample);

	segment->get_subsampled_edges(edges, start_sample, end_sample,
		samples_per_pixel / LogicSignal::Oversampling, 0);
	assert(edges.size() >= 2);

	// Check whether we need to paint the sampling points
	GlobalSettings settings;
	const bool show_sampling_points =
		settings.value(GlobalSettings::Key_View_ShowSamplingPoints).toBool() &&
		(samples_per_pixel < 0.25);

	vector<QRectF> sampling_points;
	float sampling_point_x = 0.0f;
	int64_t sampling_point_sample = start_sample;
	const int w = 2;

	if (show_sampling_points) {
		sampling_points.reserve(end_sample - start_sample + 1);
		sampling_point_x = (edges.cbegin()->first / samples_per_pixel - pixels_offset) + pp.left();
	}

	// Paint the edges
	const unsigned int edge_count = edges.size() - 2;
	QLineF *const edge_lines = new QLineF[edge_count];
	line = edge_lines;

	for (auto i = edges.cbegin() + 1; i != edges.cend() - 1; i++) {
		const float x = ((*i).first / samples_per_pixel -
			pixels_offset) + pp.left();
		*line++ = QLineF(x, high_offset, x, low_offset);

		if (show_sampling_points)
			while (sampling_point_sample < (*i).first) {
				const float y = (*i).second ? low_offset : high_offset;
				sampling_points.emplace_back(
					QRectF(sampling_point_x - (w / 2), y - (w / 2), w, w));
				sampling_point_sample++;
				sampling_point_x += pixels_per_sample;
			};
	}

	// Calculate the sample points from the last edge to the end of the trace
	if (show_sampling_points)
		while ((uint64_t)sampling_point_sample <= end_sample) {
			// Signal changed after the last edge, so the level is inverted
			const float y = (edges.cend() - 1)->second ? high_offset : low_offset;
			sampling_points.emplace_back(
				QRectF(sampling_point_x - (w / 2), y - (w / 2), w, w));
			sampling_point_sample++;
			sampling_point_x += pixels_per_sample;
		};

	p.setPen(LogicSignal::EdgeColour);
	p.drawLines(edge_lines, edge_count);
	delete[] edge_lines;

	// Paint the caps
	const unsigned int max_cap_line_count = edges.size();
	QLineF *const cap_lines = new QLineF[max_cap_line_count];

	p.setPen(LogicSignal::HighColour);
	paint_logic_caps(p, cap_lines, edges, true, samples_per_pixel,
		pixels_offset, pp.left(), high_offset);
	p.setPen(LogicSignal::LowColour);
	paint_logic_caps(p, cap_lines, edges, false, samples_per_pixel,
		pixels_offset, pp.left(), low_offset);

	delete[] cap_lines;

	// Paint the sampling points
	if (show_sampling_points) {
		p.setPen(SamplingPointColour);
		p.drawRects(sampling_points.data(), sampling_points.size());
	}
}

void AnalogSignal::paint_logic_caps(QPainter &p, QLineF *const lines,
	vector< pair<int64_t, bool> > &edges, bool level,
	double samples_per_pixel, double pixels_offset, float x_offset,
	float y_offset)
{
	QLineF *line = lines;

	for (auto i = edges.begin(); i != (edges.end() - 1); i++)
		if ((*i).second == level) {
			*line++ = QLineF(
				((*i).first / samples_per_pixel -
					pixels_offset) + x_offset, y_offset,
				((*(i+1)).first / samples_per_pixel -
					pixels_offset) + x_offset, y_offset);
		}

	p.drawLines(lines, line - lines);
}

float AnalogSignal::get_resolution(int scale_index)
{
	const float seq[] = {1.0f, 2.0f, 5.0f};

	const int offset = numeric_limits<int>::max() / (2 * countof(seq));
	const div_t d = div((int)(scale_index + countof(seq) * offset),
		countof(seq));

	return powf(10.0f, d.quot - offset) * seq[d.rem];
}

void AnalogSignal::update_scale()
{
	resolution_ = get_resolution(scale_index_);
	scale_ = div_height_ / resolution_;
}

void AnalogSignal::update_conversion_type()
{
	base_->set_conversion_type(conversion_type_);

	if (owner_)
		owner_->row_item_appearance_changed(false, true);
}

void AnalogSignal::perform_autoranging(bool keep_divs, bool force_update)
{
	const deque< shared_ptr<pv::data::AnalogSegment> > &segments =
		base_->analog_data()->analog_segments();

	if (segments.empty())
		return;

	static double prev_min = 0, prev_max = 0;
	double min = 0, max = 0;

	for (shared_ptr<pv::data::AnalogSegment> segment : segments) {
		pair<double, double> mm = segment->get_min_max();
		min = std::min(min, mm.first);
		max = std::max(max, mm.second);
	}

	if ((min == prev_min) && (max == prev_max) && !force_update)
		return;

	prev_min = min;
	prev_max = max;

	// If we're allowed to alter the div assignment...
	if (!keep_divs) {
		// Use all divs for the positive range if there are no negative values
		if ((min == 0) && (neg_vdivs_ > 0)) {
			pos_vdivs_ += neg_vdivs_;
			neg_vdivs_ = 0;
		}

		// Split up the divs if there are negative values but no negative divs
		if ((min < 0) && (neg_vdivs_ == 0)) {
			neg_vdivs_ = pos_vdivs_ / 2;
			pos_vdivs_ -= neg_vdivs_;
		}
	}

 	// If there is still no positive div when we need it, add one
	// (this can happen when pos_vdivs==neg_vdivs==0)
	if ((max > 0) && (pos_vdivs_ == 0)) {
		pos_vdivs_ = 1;
		owner_->extents_changed(false, true);
	}

	// If there is still no negative div when we need it, add one
	// (this can happen when pos_vdivs was 0 or 1 when trying to split)
	if ((min < 0) && (neg_vdivs_ == 0)) {
		neg_vdivs_ = 1;
		owner_->extents_changed(false, true);
	}

	double min_value_per_div;
	if ((pos_vdivs_ > 0) && (neg_vdivs_ >  0))
		min_value_per_div = std::max(max / pos_vdivs_, -min / neg_vdivs_);
	else if (pos_vdivs_ > 0)
		min_value_per_div = max / pos_vdivs_;
	else
		min_value_per_div = -min / neg_vdivs_;

	// Find first scale value that is bigger than the value we need
	for (int i = MinScaleIndex; i < MaxScaleIndex; i++)
		if (get_resolution(i) > min_value_per_div) {
			scale_index_ = i;
			break;
		}

	update_scale();
}

void AnalogSignal::populate_popup_form(QWidget *parent, QFormLayout *form)
{
	// Add the standard options
	Signal::populate_popup_form(parent, form);

	QFormLayout *const layout = new QFormLayout;

	// Add the number of vdivs
	pvdiv_sb_ = new QSpinBox(parent);
	pvdiv_sb_->setRange(0, MaximumVDivs);
	pvdiv_sb_->setValue(pos_vdivs_);
	connect(pvdiv_sb_, SIGNAL(valueChanged(int)),
		this, SLOT(on_pos_vdivs_changed(int)));
	layout->addRow(tr("Number of pos vertical divs"), pvdiv_sb_);

	nvdiv_sb_ = new QSpinBox(parent);
	nvdiv_sb_->setRange(0, MaximumVDivs);
	nvdiv_sb_->setValue(neg_vdivs_);
	connect(nvdiv_sb_, SIGNAL(valueChanged(int)),
		this, SLOT(on_neg_vdivs_changed(int)));
	layout->addRow(tr("Number of neg vertical divs"), nvdiv_sb_);

	// Add the vertical resolution
	resolution_cb_ = new QComboBox(parent);

	for (int i = MinScaleIndex; i < MaxScaleIndex; i++) {
		const QString label = QString("%1").arg(get_resolution(i));
		resolution_cb_->insertItem(0, label, QVariant(i));
	}

	int cur_idx = resolution_cb_->findData(QVariant(scale_index_));
	resolution_cb_->setCurrentIndex(cur_idx);

	connect(resolution_cb_, SIGNAL(currentIndexChanged(int)),
		this, SLOT(on_resolution_changed(int)));

	QGridLayout *const vdiv_layout = new QGridLayout;
	QLabel *const vdiv_unit = new QLabel(tr("V/div"));
	vdiv_layout->addWidget(resolution_cb_, 0, 0);
	vdiv_layout->addWidget(vdiv_unit, 0, 1);

	layout->addRow(tr("Vertical resolution"), vdiv_layout);

	// Add the autoranging checkbox
	QCheckBox* autoranging_cb = new QCheckBox();
	autoranging_cb->setCheckState(autoranging_ ? Qt::Checked : Qt::Unchecked);

	connect(autoranging_cb, SIGNAL(stateChanged(int)),
		this, SLOT(on_autoranging_changed(int)));

	layout->addRow(tr("Autoranging"), autoranging_cb);

	// Add the conversion type dropdown
	conversion_cb_ = new QComboBox();

	conversion_cb_->addItem("none", data::SignalBase::NoConversion);
	conversion_cb_->addItem("to logic via threshold", data::SignalBase::A2LConversionByTreshold);
	conversion_cb_->addItem("to logic via schmitt-trigger", data::SignalBase::A2LConversionBySchmittTrigger);

	cur_idx = conversion_cb_->findData(QVariant(conversion_type_));
	conversion_cb_->setCurrentIndex(cur_idx);

//	layout->addRow(tr("Conversion"), conversion_cb_);

	connect(conversion_cb_, SIGNAL(currentIndexChanged(int)),
		this, SLOT(on_conversion_changed(int)));

	// Add the display type dropdown
	display_type_cb_ = new QComboBox();

	display_type_cb_->addItem(tr("Analog"), DisplayAnalog);
	display_type_cb_->addItem(tr("Converted"), DisplayConverted);
	display_type_cb_->addItem(tr("Both"), DisplayBoth);

	cur_idx = display_type_cb_->findData(QVariant(display_type_));
	display_type_cb_->setCurrentIndex(cur_idx);

//	layout->addRow(tr("Traces to show:"), display_type_cb_);

	connect(display_type_cb_, SIGNAL(currentIndexChanged(int)),
		this, SLOT(on_display_type_changed(int)));

	form->addRow(layout);
}

void AnalogSignal::on_samples_added()
{
	perform_autoranging(false, false);
}

void AnalogSignal::on_pos_vdivs_changed(int vdivs)
{
	if (vdivs == pos_vdivs_)
		return;

	pos_vdivs_ = vdivs;

	// There has to be at least one div, positive or negative
	if ((neg_vdivs_ == 0) && (pos_vdivs_ == 0)) {
		pos_vdivs_ = 1;
		if (pvdiv_sb_)
			pvdiv_sb_->setValue(pos_vdivs_);
	}

	if (autoranging_) {
		perform_autoranging(true, true);

		// It could be that a positive or negative div was added, so update
		if (pvdiv_sb_) {
			pvdiv_sb_->setValue(pos_vdivs_);
			nvdiv_sb_->setValue(neg_vdivs_);
		}
	}

	if (owner_) {
		// Call order is important, otherwise the lazy event handler won't work
		owner_->extents_changed(false, true);
		owner_->row_item_appearance_changed(false, true);
	}
}

void AnalogSignal::on_neg_vdivs_changed(int vdivs)
{
	if (vdivs == neg_vdivs_)
		return;

	neg_vdivs_ = vdivs;

	// There has to be at least one div, positive or negative
	if ((neg_vdivs_ == 0) && (pos_vdivs_ == 0)) {
		pos_vdivs_ = 1;
		if (pvdiv_sb_)
			pvdiv_sb_->setValue(pos_vdivs_);
	}

	if (autoranging_) {
		perform_autoranging(true, true);

		// It could be that a positive or negative div was added, so update
		if (pvdiv_sb_) {
			pvdiv_sb_->setValue(pos_vdivs_);
			nvdiv_sb_->setValue(neg_vdivs_);
		}
	}

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

void AnalogSignal::on_autoranging_changed(int state)
{
	autoranging_ = (state == Qt::Checked);

	if (autoranging_)
		perform_autoranging(false, true);

	if (owner_) {
		// Call order is important, otherwise the lazy event handler won't work
		owner_->extents_changed(false, true);
		owner_->row_item_appearance_changed(false, true);
	}
}

void AnalogSignal::on_conversion_changed(int index)
{
	data::SignalBase::ConversionType old_conv_type = conversion_type_;

	conversion_type_ = (data::SignalBase::ConversionType)(conversion_cb_->itemData(index).toInt());

	if (conversion_type_ != old_conv_type) {
		base_->set_conversion_type(conversion_type_);
		update_conversion_type();
	}
}

void AnalogSignal::on_display_type_changed(int index)
{
	display_type_ = (DisplayType)(display_type_cb_->itemData(index).toInt());

	if (owner_)
		owner_->row_item_appearance_changed(false, true);
}

} // namespace TraceView
} // namespace views
} // namespace pv
