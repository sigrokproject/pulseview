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

#include <algorithm>

#include <QApplication>
#include <QFormLayout>
#include <QToolBar>

#include "logicsignal.hpp"
#include "view.hpp"

#include <pv/data/logic.hpp>
#include <pv/data/logicsegment.hpp>
#include <pv/data/signalbase.hpp>
#include <pv/devicemanager.hpp>
#include <pv/devices/device.hpp>
#include <pv/globalsettings.hpp>
#include <pv/session.hpp>

#include <libsigrokcxx/libsigrokcxx.hpp>

using std::deque;
using std::max;
using std::make_pair;
using std::min;
using std::none_of;
using std::out_of_range;
using std::pair;
using std::shared_ptr;
using std::vector;

using sigrok::ConfigKey;
using sigrok::Capability;
using sigrok::Trigger;
using sigrok::TriggerMatch;
using sigrok::TriggerMatchType;

using pv::data::LogicSegment;

namespace pv {
namespace views {
namespace trace {

const float LogicSignal::Oversampling = 2.0f;

const QColor LogicSignal::EdgeColor(0x80, 0x80, 0x80);
const QColor LogicSignal::HighColor(0x00, 0xC0, 0x00);
const QColor LogicSignal::LowColor(0xC0, 0x00, 0x00);
const QColor LogicSignal::SamplingPointColor(0x77, 0x77, 0x77);

const QColor LogicSignal::SignalColors[10] = {
	QColor(0x16, 0x19, 0x1A),	// Black
	QColor(0x8F, 0x52, 0x02),	// Brown
	QColor(0xCC, 0x00, 0x00),	// Red
	QColor(0xF5, 0x79, 0x00),	// Orange
	QColor(0xED, 0xD4, 0x00),	// Yellow
	QColor(0x73, 0xD2, 0x16),	// Green
	QColor(0x34, 0x65, 0xA4),	// Blue
	QColor(0x75, 0x50, 0x7B),	// Violet
	QColor(0x88, 0x8A, 0x85),	// Grey
	QColor(0xEE, 0xEE, 0xEC),	// White
};

QColor LogicSignal::TriggerMarkerBackgroundColor = QColor(0xED, 0xD4, 0x00);
const int LogicSignal::TriggerMarkerPadding = 2;
const char* LogicSignal::TriggerMarkerIcons[8] = {
	nullptr,
	":/icons/trigger-marker-low.svg",
	":/icons/trigger-marker-high.svg",
	":/icons/trigger-marker-rising.svg",
	":/icons/trigger-marker-falling.svg",
	":/icons/trigger-marker-change.svg",
	nullptr,
	nullptr
};

QCache<QString, const QIcon> LogicSignal::icon_cache_;
QCache<QString, const QPixmap> LogicSignal::pixmap_cache_;

LogicSignal::LogicSignal(
	pv::Session &session,
	shared_ptr<devices::Device> device,
	shared_ptr<data::SignalBase> base) :
	Signal(session, base),
	device_(device),
	trigger_types_(get_trigger_types()),
	trigger_none_(nullptr),
	trigger_rising_(nullptr),
	trigger_high_(nullptr),
	trigger_falling_(nullptr),
	trigger_low_(nullptr),
	trigger_change_(nullptr)
{
	shared_ptr<Trigger> trigger;

	base_->set_color(SignalColors[base->index() % countof(SignalColors)]);

	GlobalSettings gs;
	signal_height_ = gs.value(GlobalSettings::Key_View_DefaultLogicHeight).toInt();

	/* Populate this channel's trigger setting with whatever we
	 * find in the current session trigger, if anything. */
	trigger_match_ = nullptr;
	if ((trigger = session_.session()->trigger()))
		for (auto stage : trigger->stages())
			for (auto match : stage->matches())
				if (match->channel() == base_->channel())
					trigger_match_ = match->type();
}

shared_ptr<pv::data::SignalData> LogicSignal::data() const
{
	return base_->logic_data();
}

shared_ptr<pv::data::Logic> LogicSignal::logic_data() const
{
	return base_->logic_data();
}

void LogicSignal::save_settings(QSettings &settings) const
{
	settings.setValue("trace_height", signal_height_);
}

void LogicSignal::restore_settings(QSettings &settings)
{
	if (settings.contains("trace_height")) {
		const int old_height = signal_height_;
		signal_height_ = settings.value("trace_height").toInt();

		if ((signal_height_ != old_height) && owner_) {
			// Call order is important, otherwise the lazy event handler won't work
			owner_->extents_changed(false, true);
			owner_->row_item_appearance_changed(false, true);
		}
	}
}

pair<int, int> LogicSignal::v_extents() const
{
	const int signal_margin =
		QFontMetrics(QApplication::font()).height() / 2;
	return make_pair(-signal_height_ - signal_margin, signal_margin);
}

void LogicSignal::paint_mid(QPainter &p, ViewItemPaintParams &pp)
{
	QLineF *line;

	vector< pair<int64_t, bool> > edges;

	assert(base_);
	assert(owner_);

	const int y = get_visual_y();

	if (!base_->enabled())
		return;

	const float low_offset = y + 0.5f;
	const float high_offset = low_offset - signal_height_;

	shared_ptr<LogicSegment> segment = get_logic_segment_to_paint();
	if (!segment || (segment->get_sample_count() == 0))
		return;

	double samplerate = segment->samplerate();

	// Show sample rate as 1Hz when it is unknown
	if (samplerate == 0.0)
		samplerate = 1.0;

	const double pixels_offset = pp.pixels_offset();
	const pv::util::Timestamp& start_time = segment->start_time();
	const int64_t last_sample = (int64_t)segment->get_sample_count() - 1;
	const double samples_per_pixel = samplerate * pp.scale();
	const double pixels_per_sample = 1 / samples_per_pixel;
	const pv::util::Timestamp start = samplerate * (pp.offset() - start_time);
	const pv::util::Timestamp end = start + samples_per_pixel * pp.width();

	const int64_t start_sample = min(max(floor(start).convert_to<int64_t>(),
		(int64_t)0), last_sample);
	const uint64_t end_sample = min(max(ceil(end).convert_to<int64_t>(),
		(int64_t)0), last_sample);

	segment->get_subsampled_edges(edges, start_sample, end_sample,
		samples_per_pixel / Oversampling, base_->index());
	assert(edges.size() >= 2);

	const float first_sample_x =
		pp.left() + (edges.front().first / samples_per_pixel - pixels_offset);
	const float last_sample_x =
		pp.left() + (edges.back().first / samples_per_pixel - pixels_offset);

	// Check whether we need to paint the sampling points
	GlobalSettings settings;
	const bool show_sampling_points =
		settings.value(GlobalSettings::Key_View_ShowSamplingPoints).toBool() &&
		(samples_per_pixel < 0.25);

	vector<QRectF> sampling_points;
	float sampling_point_x = first_sample_x;
	int64_t sampling_point_sample = start_sample;
	const int w = 2;

	if (show_sampling_points)
		sampling_points.reserve(end_sample - start_sample + 1);

	// Check whether we need to fill the high areas
	const bool fill_high_areas =
		settings.value(GlobalSettings::Key_View_FillSignalHighAreas).toBool();
	vector<QRectF> high_rects;
	float rising_edge_x;
	bool rising_edge_seen = false;

	// Paint the edges
	const unsigned int edge_count = edges.size() - 2;
	QLineF *const edge_lines = new QLineF[edge_count];
	line = edge_lines;

	if (edges.front().second) {
		// Beginning of trace is high
		rising_edge_x = first_sample_x;
		rising_edge_seen = true;
	}

	for (auto i = edges.cbegin() + 1; i != edges.cend() - 1; i++) {
		// Note: multiple edges occupying a single pixel are represented by an edge
		// with undefined logic level. This means that only the first falling edge
		// after a rising edge corresponds to said rising edge - and vice versa. If
		// more edges with the same logic level follow, they denote multiple edges.

		const float x = pp.left() + ((*i).first / samples_per_pixel - pixels_offset);
		*line++ = QLineF(x, high_offset, x, low_offset);

		if (fill_high_areas) {
			// Any edge terminates a high area
			if (rising_edge_seen) {
				const int width = x - rising_edge_x;
				if (width > 0)
					high_rects.emplace_back(rising_edge_x, high_offset,
						width, signal_height_);
				rising_edge_seen = false;
			}

			// Only rising edges start high areas
			if ((*i).second) {
				rising_edge_x = x;
				rising_edge_seen = true;
			}
		}

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

	if (fill_high_areas) {
		// Add last high rectangle if the signal is still high at the end of the trace
		if (rising_edge_seen && (edges.cend() - 1)->second)
			high_rects.emplace_back(rising_edge_x, high_offset,
				last_sample_x - rising_edge_x, signal_height_);

		const QColor fill_color = QColor::fromRgba(settings.value(
			GlobalSettings::Key_View_FillSignalHighAreaColor).value<uint32_t>());
		p.setPen(fill_color);
		p.setBrush(fill_color);
		p.drawRects((const QRectF*)(high_rects.data()), high_rects.size());
	}

	p.setPen(EdgeColor);
	p.drawLines(edge_lines, edge_count);
	delete[] edge_lines;

	// Paint the caps
	const unsigned int max_cap_line_count = edges.size();
	QLineF *const cap_lines = new QLineF[max_cap_line_count];

	p.setPen(HighColor);
	paint_caps(p, cap_lines, edges, true, samples_per_pixel,
		pixels_offset, pp.left(), high_offset);
	p.setPen(LowColor);
	paint_caps(p, cap_lines, edges, false, samples_per_pixel,
		pixels_offset, pp.left(), low_offset);

	delete[] cap_lines;

	// Paint the sampling points
	if (show_sampling_points) {
		p.setPen(SamplingPointColor);
		p.drawRects(sampling_points.data(), sampling_points.size());
	}
}

void LogicSignal::paint_fore(QPainter &p, ViewItemPaintParams &pp)
{
	if (base_->enabled()) {
		if (trigger_match_) {
			// Draw the trigger marker
			const int y = get_visual_y();

			for (int32_t type_id : trigger_types_) {
				const TriggerMatchType *const type =
					TriggerMatchType::get(type_id);
				if (trigger_match_ != type || type_id < 0 ||
					(size_t)type_id >= countof(TriggerMarkerIcons) ||
					!TriggerMarkerIcons[type_id])
					continue;

				const QPixmap *const pixmap = get_pixmap(
					TriggerMarkerIcons[type_id]);
				if (!pixmap)
					continue;

				const float pad = TriggerMarkerPadding - 0.5f;
				const QSize size = pixmap->size();
				const QPoint point(
					pp.right() - size.width() - pad * 2,
					y - (signal_height_ + size.height()) / 2);

				p.setPen(QPen(TriggerMarkerBackgroundColor.darker()));
				p.setBrush(TriggerMarkerBackgroundColor);
				p.drawRoundedRect(QRectF(point, size).adjusted(
					-pad, -pad, pad, pad), pad, pad);
				p.drawPixmap(point, *pixmap);

				break;
			}
		}

		if (show_hover_marker_)
			paint_hover_marker(p);
	}
}

vector<LogicSegment::EdgePair> LogicSignal::get_nearest_level_changes(uint64_t sample_pos)
{
	assert(base_);
	assert(owner_);

	if (sample_pos == 0)
		return vector<LogicSegment::EdgePair>();

	shared_ptr<LogicSegment> segment = get_logic_segment_to_paint();
	if (!segment || (segment->get_sample_count() == 0))
		return vector<LogicSegment::EdgePair>();

	const View *view = owner_->view();
	assert(view);
	const double samples_per_pixel = base_->get_samplerate() * view->scale();

	vector<LogicSegment::EdgePair> edges;

	segment->get_surrounding_edges(edges, sample_pos,
		samples_per_pixel / Oversampling, base_->index());

	if (edges.empty())
		return vector<LogicSegment::EdgePair>();

	return edges;
}

void LogicSignal::paint_caps(QPainter &p, QLineF *const lines,
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

shared_ptr<pv::data::LogicSegment> LogicSignal::get_logic_segment_to_paint() const
{
	shared_ptr<pv::data::LogicSegment> segment;

	const deque< shared_ptr<pv::data::LogicSegment> > &segments =
		base_->logic_data()->logic_segments();

	if (!segments.empty()) {
		if (segment_display_mode_ == ShowLastSegmentOnly) {
			segment = segments.back();
		}

	if ((segment_display_mode_ == ShowSingleSegmentOnly) ||
		(segment_display_mode_ == ShowLastCompleteSegmentOnly)) {
			try {
				segment = segments.at(current_segment_);
			} catch (out_of_range&) {
				qDebug() << "Current logic segment out of range for signal" << base_->name() << ":" << current_segment_;
			}
		}
	}

	return segment;
}

void LogicSignal::init_trigger_actions(QWidget *parent)
{
	trigger_none_ = new QAction(*get_icon(":/icons/trigger-none.svg"),
		tr("No trigger"), parent);
	trigger_none_->setCheckable(true);
	connect(trigger_none_, SIGNAL(triggered()), this, SLOT(on_trigger()));

	trigger_rising_ = new QAction(*get_icon(":/icons/trigger-rising.svg"),
		tr("Trigger on rising edge"), parent);
	trigger_rising_->setCheckable(true);
	connect(trigger_rising_, SIGNAL(triggered()), this, SLOT(on_trigger()));

	trigger_high_ = new QAction(*get_icon(":/icons/trigger-high.svg"),
		tr("Trigger on high level"), parent);
	trigger_high_->setCheckable(true);
	connect(trigger_high_, SIGNAL(triggered()), this, SLOT(on_trigger()));

	trigger_falling_ = new QAction(*get_icon(":/icons/trigger-falling.svg"),
		tr("Trigger on falling edge"), parent);
	trigger_falling_->setCheckable(true);
	connect(trigger_falling_, SIGNAL(triggered()), this, SLOT(on_trigger()));

	trigger_low_ = new QAction(*get_icon(":/icons/trigger-low.svg"),
		tr("Trigger on low level"), parent);
	trigger_low_->setCheckable(true);
	connect(trigger_low_, SIGNAL(triggered()), this, SLOT(on_trigger()));

	trigger_change_ = new QAction(*get_icon(":/icons/trigger-change.svg"),
		tr("Trigger on rising or falling edge"), parent);
	trigger_change_->setCheckable(true);
	connect(trigger_change_, SIGNAL(triggered()), this, SLOT(on_trigger()));
}

const vector<int32_t> LogicSignal::get_trigger_types() const
{
	// We may not be associated with a device
	if (!device_)
		return vector<int32_t>();

	const auto sr_dev = device_->device();
	if (sr_dev->config_check(ConfigKey::TRIGGER_MATCH, Capability::LIST)) {
		const Glib::VariantContainerBase gvar =
			sr_dev->config_list(ConfigKey::TRIGGER_MATCH);

		vector<int32_t> ttypes;

		for (unsigned int i = 0; i < gvar.get_n_children(); i++) {
			Glib::VariantBase tmp_vb;
			gvar.get_child(tmp_vb, i);

			Glib::Variant<int32_t> tmp_v =
				Glib::VariantBase::cast_dynamic< Glib::Variant<int32_t> >(tmp_vb);

			ttypes.push_back(tmp_v.get());
		}

		return ttypes;
	} else {
		return vector<int32_t>();
	}
}

QAction* LogicSignal::action_from_trigger_type(const TriggerMatchType *type)
{
	QAction *action;

	action = trigger_none_;
	if (type) {
		switch (type->id()) {
		case SR_TRIGGER_ZERO:
			action = trigger_low_;
			break;
		case SR_TRIGGER_ONE:
			action = trigger_high_;
			break;
		case SR_TRIGGER_RISING:
			action = trigger_rising_;
			break;
		case SR_TRIGGER_FALLING:
			action = trigger_falling_;
			break;
		case SR_TRIGGER_EDGE:
			action = trigger_change_;
			break;
		default:
			assert(false);
		}
	}

	return action;
}

const TriggerMatchType *LogicSignal::trigger_type_from_action(QAction *action)
{
	if (action == trigger_low_)
		return TriggerMatchType::ZERO;
	else if (action == trigger_high_)
		return TriggerMatchType::ONE;
	else if (action == trigger_rising_)
		return TriggerMatchType::RISING;
	else if (action == trigger_falling_)
		return TriggerMatchType::FALLING;
	else if (action == trigger_change_)
		return TriggerMatchType::EDGE;
	else
		return nullptr;
}

void LogicSignal::populate_popup_form(QWidget *parent, QFormLayout *form)
{
	Signal::populate_popup_form(parent, form);

	signal_height_sb_ = new QSpinBox(parent);
	signal_height_sb_->setRange(5, 1000);
	signal_height_sb_->setSingleStep(5);
	signal_height_sb_->setSuffix(tr(" pixels"));
	signal_height_sb_->setValue(signal_height_);
	connect(signal_height_sb_, SIGNAL(valueChanged(int)),
		this, SLOT(on_signal_height_changed(int)));
	form->addRow(tr("Trace height"), signal_height_sb_);

	// Trigger settings
	const vector<int32_t> trig_types = get_trigger_types();

	if (!trig_types.empty()) {
		trigger_bar_ = new QToolBar(parent);
		init_trigger_actions(trigger_bar_);
		trigger_bar_->addAction(trigger_none_);
		trigger_none_->setChecked(!trigger_match_);

		for (auto type_id : trig_types) {
			const TriggerMatchType *const type =
				TriggerMatchType::get(type_id);
			QAction *const action = action_from_trigger_type(type);
			trigger_bar_->addAction(action);
			action->setChecked(trigger_match_ == type);
		}

		// Only allow triggers to be changed when we're stopped
		if (session_.get_capture_state() != Session::Stopped)
			for (QAction* action : trigger_bar_->findChildren<QAction*>())
				action->setEnabled(false);

		form->addRow(tr("Trigger"), trigger_bar_);
	}
}

void LogicSignal::modify_trigger()
{
	auto trigger = session_.session()->trigger();
	auto new_trigger = session_.device_manager().context()->create_trigger("pulseview");

	if (trigger) {
		for (auto stage : trigger->stages()) {
			const auto &matches = stage->matches();
			if (none_of(matches.begin(), matches.end(),
			    [&](shared_ptr<TriggerMatch> match) {
					return match->channel() != base_->channel(); }))
				continue;

			auto new_stage = new_trigger->add_stage();
			for (auto match : stage->matches()) {
				if (match->channel() == base_->channel())
					continue;
				new_stage->add_match(match->channel(), match->type());
			}
		}
	}

	if (trigger_match_) {
		// Until we can let the user decide how to group trigger matches
		// into stages, put all of the matches into a single stage --
		// most devices only support a single trigger stage.
		if (new_trigger->stages().empty())
			new_trigger->add_stage();

		new_trigger->stages().back()->add_match(base_->channel(),
			trigger_match_);
	}

	session_.session()->set_trigger(
		new_trigger->stages().empty() ? nullptr : new_trigger);

	if (owner_)
		owner_->row_item_appearance_changed(false, true);
}

const QIcon* LogicSignal::get_icon(const char *path)
{
	if (!icon_cache_.contains(path)) {
		const QIcon *icon = new QIcon(path);
		icon_cache_.insert(path, icon);
	}

	return icon_cache_.take(path);
}

const QPixmap* LogicSignal::get_pixmap(const char *path)
{
	if (!pixmap_cache_.contains(path)) {
		const QPixmap *pixmap = new QPixmap(path);
		pixmap_cache_.insert(path, pixmap);
	}

	return pixmap_cache_.take(path);
}

void LogicSignal::on_trigger()
{
	QAction *action;

	action_from_trigger_type(trigger_match_)->setChecked(false);

	action = (QAction *)sender();
	action->setChecked(true);
	trigger_match_ = trigger_type_from_action(action);

	modify_trigger();
}

void LogicSignal::on_signal_height_changed(int height)
{
	signal_height_ = height;

	if (owner_) {
		// Call order is important, otherwise the lazy event handler won't work
		owner_->extents_changed(false, true);
		owner_->row_item_appearance_changed(false, true);
	}
}

} // namespace trace
} // namespace views
} // namespace pv
