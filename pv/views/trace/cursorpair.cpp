/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2013 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#include <algorithm>
#include <cassert>

#include <QColor>
#include <QMenu>
#include <QToolTip>

#include "cursorpair.hpp"

#include "pv/globalsettings.hpp"
#include "pv/util.hpp"
#include "ruler.hpp"
#include "view.hpp"

using std::max;
using std::make_pair;
using std::min;
using std::shared_ptr;
using std::pair;

namespace pv {
namespace views {
namespace trace {

const int CursorPair::DeltaPadding = 8;

CursorPair::CursorPair(View &view) :
	TimeItem(view),
	first_(new Cursor(view, 0.0)),
	second_(new Cursor(view, 1.0)),
	label_incomplete_(true)
{
	GlobalSettings::add_change_handler(this);

	GlobalSettings settings;
	fill_color_ = QColor::fromRgba(settings.value(
		GlobalSettings::Key_View_CursorFillColor).value<uint32_t>());
	show_frequency_ = settings.value(
		GlobalSettings::Key_View_CursorShowFrequency).value<bool>();
	show_interval_ = settings.value(
		GlobalSettings::Key_View_CursorShowInterval).value<bool>();
	show_samples_ = settings.value(
		GlobalSettings::Key_View_CursorShowSamples).value<bool>();

	connect(&view_, SIGNAL(hover_point_changed(const QWidget*, QPoint)),
		this, SLOT(on_hover_point_changed(const QWidget*, QPoint)));
}

CursorPair::~CursorPair()
{
	GlobalSettings::remove_change_handler(this);
}

bool CursorPair::enabled() const
{
	return view_.cursors_shown();
}

shared_ptr<Cursor> CursorPair::first() const
{
	return first_;
}

shared_ptr<Cursor> CursorPair::second() const
{
	return second_;
}

void CursorPair::set_time(const pv::util::Timestamp& time)
{
	const pv::util::Timestamp delta = second_->time() - first_->time();
	first_->set_time(time);
	second_->set_time(time + delta);
}

const pv::util::Timestamp CursorPair::time() const
{
	return 0;
}

float CursorPair::get_x() const
{
	return (first_->get_x() + second_->get_x()) / 2.0f;
}

const pv::util::Timestamp CursorPair::delta(const pv::util::Timestamp& other) const
{
	if (other < second_->time())
		return other - first_->time();
	else
		return other - second_->time();
}

QPoint CursorPair::drag_point(const QRect &rect) const
{
	return first_->drag_point(rect);
}

pv::widgets::Popup* CursorPair::create_popup(QWidget *parent)
{
	(void)parent;
	return nullptr;
}

QMenu *CursorPair::create_header_context_menu(QWidget *parent)
{
	QMenu *menu = new QMenu(parent);

	QAction *displayIntervalAction = new QAction(tr("Display interval"), this);
	displayIntervalAction->setCheckable(true);
	displayIntervalAction->setChecked(show_interval_);
	menu->addAction(displayIntervalAction);

	connect(displayIntervalAction, &QAction::toggled, displayIntervalAction,
		[=]{
			GlobalSettings settings;
			settings.setValue(GlobalSettings::Key_View_CursorShowInterval,
				!settings.value(GlobalSettings::Key_View_CursorShowInterval).value<bool>());
		});

	QAction *displayFrequencyAction = new QAction(tr("Display frequency"), this);
	displayFrequencyAction->setCheckable(true);
	displayFrequencyAction->setChecked(show_frequency_);
	menu->addAction(displayFrequencyAction);

	connect(displayFrequencyAction, &QAction::toggled, displayFrequencyAction,
		[=]{
			GlobalSettings settings;
			settings.setValue(GlobalSettings::Key_View_CursorShowFrequency,
				!settings.value(GlobalSettings::Key_View_CursorShowFrequency).value<bool>());
		});

	QAction *displaySamplesAction = new QAction(tr("Display samples"), this);
	displaySamplesAction->setCheckable(true);
	displaySamplesAction->setChecked(show_samples_);
	menu->addAction(displaySamplesAction);

	connect(displaySamplesAction, &QAction::toggled, displaySamplesAction,
		[=]{
			GlobalSettings settings;
			settings.setValue(GlobalSettings::Key_View_CursorShowSamples,
				!settings.value(GlobalSettings::Key_View_CursorShowSamples).value<bool>());
		});

	return menu;
}

QRectF CursorPair::label_rect(const QRectF &rect) const
{
	const QSizeF label_size(text_size_ + LabelPadding * 2);
	const pair<float, float> offsets(get_cursor_offsets());
	const pair<float, float> normal_offsets(
		(offsets.first < offsets.second) ? offsets :
		make_pair(offsets.second, offsets.first));

	const float height = label_size.height();
	const float left = max(normal_offsets.first + DeltaPadding, -height);
	const float right = min(normal_offsets.second - DeltaPadding,
		(float)rect.width() + height);

	return QRectF(left, rect.height() - label_size.height() -
		TimeMarker::ArrowSize - 0.5f,
		right - left, height);
}

void CursorPair::paint_label(QPainter &p, const QRect &rect, bool hover)
{
	assert(first_);
	assert(second_);

	if (!enabled())
		return;

	const QColor text_color = ViewItem::select_text_color(Cursor::FillColor);
	p.setPen(text_color);

	QRectF delta_rect(label_rect(rect));
	const int radius = delta_rect.height() / 2;
	QRectF text_rect(delta_rect.intersected(rect).adjusted(radius, 0, -radius, 0));

	QString text = format_string(text_rect.width(),
		[&p](const QString& s) -> double { return p.boundingRect(QRectF(), 0, s).width(); });

	text_size_ = p.boundingRect(QRectF(), 0, text).size();

	if (selected()) {
		p.setBrush(Qt::transparent);
		p.setPen(highlight_pen());
		p.drawRoundedRect(delta_rect, radius, radius);
	}

	p.setBrush(hover ? Cursor::FillColor.lighter() : Cursor::FillColor);
	p.setPen(Cursor::FillColor.darker());
	p.drawRoundedRect(delta_rect, radius, radius);

	delta_rect.adjust(1, 1, -1, -1);
	p.setPen(Cursor::FillColor.lighter());
	const int highlight_radius = delta_rect.height() / 2 - 2;
	p.drawRoundedRect(delta_rect, highlight_radius, highlight_radius);
	label_area_ = delta_rect;

	p.setPen(text_color);
	p.drawText(text_rect, Qt::AlignCenter | Qt::AlignVCenter, text);
}

void CursorPair::paint_back(QPainter &p, ViewItemPaintParams &pp)
{
	if (!enabled())
		return;

	p.setPen(Qt::NoPen);
	p.setBrush(fill_color_);

	const pair<float, float> offsets(get_cursor_offsets());
	const int l = (int)max(min(offsets.first, offsets.second), 0.0f);
	const int r = (int)min(max(offsets.first, offsets.second), (float)pp.width());

	p.drawRect(l, pp.top(), r - l, pp.height());
}

QString CursorPair::format_string(int max_width, std::function<double(const QString&)> query_size)
{
	int time_precision = 12;
	int freq_precision = 12;

	QString s = format_string_sub(time_precision, freq_precision);

	// Try full "{time} s / {freq} Hz" format
	if ((max_width <= 0) || (query_size(s) <= max_width)) {
		label_incomplete_ = false;
		return s;
	}

	label_incomplete_ = true;

	// Gradually reduce time precision to match frequency precision
	while (time_precision > freq_precision) {
		time_precision--;

		s = format_string_sub(time_precision, freq_precision);
		if (query_size(s) <= max_width)
			return s;
	}

	// Gradually reduce both precisions down to zero
	while (time_precision > 0) {
		time_precision--;
		freq_precision--;

		s = format_string_sub(time_precision, freq_precision);
		if (query_size(s) <= max_width)
			return s;
	}

	// Try no trailing digits and drop the unit to at least display something
	s = format_string_sub(0, 0, false);

	if (query_size(s) <= max_width)
		return s;

	// Give up
	return "...";
}

pair<float, float> CursorPair::get_cursor_offsets() const
{
	assert(first_);
	assert(second_);

	return pair<float, float>(first_->get_x(), second_->get_x());
}

void CursorPair::on_setting_changed(const QString &key, const QVariant &value)
{
	if (key == GlobalSettings::Key_View_CursorFillColor)
		fill_color_ = QColor::fromRgba(value.value<uint32_t>());

	if (key == GlobalSettings::Key_View_CursorShowFrequency)
		show_frequency_ = value.value<bool>();

	if (key == GlobalSettings::Key_View_CursorShowInterval)
		show_interval_ = value.value<bool>();

	if (key == GlobalSettings::Key_View_CursorShowSamples)
		show_samples_ = value.value<bool>();
}

void CursorPair::on_hover_point_changed(const QWidget* widget, const QPoint& hp)
{
	if (widget != view_.ruler())
		return;

	if (!label_incomplete_)
		return;

	if (label_area_.contains(hp))
		QToolTip::showText(view_.mapToGlobal(hp), format_string());
	else
		QToolTip::hideText();  // TODO Will break other tooltips when there can be others
}

QString CursorPair::format_string_sub(int time_precision, int freq_precision, bool show_unit)
{
	QString s = " ";

	const pv::util::SIPrefix prefix = view_.tick_prefix();
	const pv::util::Timestamp diff = abs(second_->time() - first_->time());

	const QString time = Ruler::format_time_with_distance(
		diff, diff, prefix, (show_unit ? view_.time_unit() : pv::util::TimeUnit::None),
		time_precision, false);

	// We can only show a frequency when there's a time base
	if (view_.time_unit() == pv::util::TimeUnit::Time) {
		int items = 0;

		if (show_frequency_) {
			const QString freq = util::format_value_si(
				1 / diff.convert_to<double>(), pv::util::SIPrefix::unspecified,
				freq_precision, (show_unit ? "Hz" : nullptr), false);
			s = QString("%1").arg(freq);
			items++;
		}

		if (show_interval_) {
			if (items > 0)
				s = QString("%1 / %2").arg(s, time);
			else
				s = QString("%1").arg(time);
			items++;
		}

		if (show_samples_) {
			const QString samples = QString::number(
				(diff * view_.session().get_samplerate()).convert_to<uint64_t>());
			if (items > 0)
				s = QString("%1 / %2").arg(s, samples);
			else
				s = QString("%1").arg(samples);
		}
	} else
		// In this case, we return the number of samples, really
		s = time;

	return s;
}

} // namespace trace
} // namespace views
} // namespace pv
