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

extern "C" {
#include <libsigrokdecode/libsigrokdecode.h>
}

#include <limits>
#include <mutex>
#include <tuple>

#include <extdef.h>

#include <boost/functional/hash.hpp>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QTextStream>
#include <QToolTip>

#include "decodetrace.hpp"
#include "view.hpp"
#include "viewport.hpp"

#include <pv/globalsettings.hpp>
#include <pv/session.hpp>
#include <pv/strnatcmp.hpp>
#include <pv/data/decodesignal.hpp>
#include <pv/data/decode/annotation.hpp>
#include <pv/data/decode/decoder.hpp>
#include <pv/data/logic.hpp>
#include <pv/data/logicsegment.hpp>
#include <pv/widgets/decodergroupbox.hpp>
#include <pv/widgets/decodermenu.hpp>

using std::abs;
using std::find_if;
using std::lock_guard;
using std::make_pair;
using std::max;
using std::min;
using std::numeric_limits;
using std::out_of_range;
using std::pair;
using std::shared_ptr;
using std::tie;
using std::vector;

using pv::data::decode::Annotation;
using pv::data::decode::Row;
using pv::data::decode::DecodeChannel;
using pv::data::DecodeSignal;

namespace pv {
namespace views {
namespace trace {


#define DECODETRACE_COLOR_SATURATION (180) /* 0-255 */
#define DECODETRACE_COLOR_VALUE (170) /* 0-255 */

const QColor DecodeTrace::ErrorBgColor = QColor(0xEF, 0x29, 0x29);
const QColor DecodeTrace::NoDecodeColor = QColor(0x88, 0x8A, 0x85);
const uint8_t DecodeTrace::ExpansionAreaHeaderAlpha = 10 * 255 / 100;
const uint8_t DecodeTrace::ExpansionAreaAlpha = 5 * 255 / 100;

const int DecodeTrace::ArrowSize = 6;
const double DecodeTrace::EndCapWidth = 5;
const int DecodeTrace::RowTitleMargin = 7;
const int DecodeTrace::DrawPadding = 100;

const int DecodeTrace::MaxTraceUpdateRate = 1; // No more than 1 Hz
const unsigned int DecodeTrace::AnimationDurationInTicks = 7;

DecodeTrace::DecodeTrace(pv::Session &session,
	shared_ptr<data::SignalBase> signalbase, int index) :
	Trace(signalbase),
	session_(session),
	max_visible_rows_(0),
	delete_mapper_(this),
	show_hide_mapper_(this),
	row_show_hide_mapper_(this)
{
	decode_signal_ = dynamic_pointer_cast<data::DecodeSignal>(base_);

	GlobalSettings settings;
	always_show_all_rows_ = settings.value(GlobalSettings::Key_Dec_AlwaysShowAllRows).toBool();

	GlobalSettings::add_change_handler(this);

	// Determine shortest string we want to see displayed in full
	QFontMetrics m(QApplication::font());
	min_useful_label_width_ = m.width("XX"); // e.g. two hex characters

	default_row_height_ = (ViewItemPaintParams::text_height() * 6) / 4;
	annotation_height_ = (ViewItemPaintParams::text_height() * 5) / 4;

	// For the base color, we want to start at a very different color for
	// every decoder stack, so multiply the index with a number that is
	// rather close to 180 degrees of the color circle but not a dividend of 360
	// Note: The offset equals the color of the first annotation
	QColor color;
	const int h = (120 + 160 * index) % 360;
	const int s = DECODETRACE_COLOR_SATURATION;
	const int v = DECODETRACE_COLOR_VALUE;
	color.setHsv(h, s, v);
	base_->set_color(color);

	connect(decode_signal_.get(), SIGNAL(new_annotations()),
		this, SLOT(on_new_annotations()));
	connect(decode_signal_.get(), SIGNAL(decode_reset()),
		this, SLOT(on_decode_reset()));
	connect(decode_signal_.get(), SIGNAL(decode_finished()),
		this, SLOT(on_decode_finished()));
	connect(decode_signal_.get(), SIGNAL(channels_updated()),
		this, SLOT(on_channels_updated()));

	connect(&delete_mapper_, SIGNAL(mapped(int)),
		this, SLOT(on_delete_decoder(int)));
	connect(&show_hide_mapper_, SIGNAL(mapped(int)),
		this, SLOT(on_show_hide_decoder(int)));
	connect(&row_show_hide_mapper_, SIGNAL(mapped(int)),
		this, SLOT(on_show_hide_row(int)));

	connect(&delayed_trace_updater_, SIGNAL(timeout()),
		this, SLOT(on_delayed_trace_update()));
	delayed_trace_updater_.setSingleShot(true);
	delayed_trace_updater_.setInterval(1000 / MaxTraceUpdateRate);

	connect(&animation_timer_, SIGNAL(timeout()),
		this, SLOT(on_animation_timer()));
	animation_timer_.setInterval(1000 / 50);

	default_marker_shape_ << QPoint(0,         -ArrowSize);
	default_marker_shape_ << QPoint(ArrowSize,  0);
	default_marker_shape_ << QPoint(0,          ArrowSize);
}

DecodeTrace::~DecodeTrace()
{
	GlobalSettings::remove_change_handler(this);

	for (RowData& r : rows_) {
		for (QCheckBox* cb : r.selectors)
			delete cb;

		delete r.selector_container;
		delete r.header_container;
		delete r.container;
	}
}

bool DecodeTrace::enabled() const
{
	return true;
}

shared_ptr<data::SignalBase> DecodeTrace::base() const
{
	return base_;
}

pair<int, int> DecodeTrace::v_extents() const
{
	// Make an empty decode trace appear symmetrical
	if (max_visible_rows_ == 0)
		return make_pair(-default_row_height_, default_row_height_);

	unsigned int height = 0;
	for (const RowData& r : rows_)
		if (r.currently_visible)
			height += r.height;

	return make_pair(-default_row_height_, height);
}

void DecodeTrace::paint_back(QPainter &p, ViewItemPaintParams &pp)
{
	Trace::paint_back(p, pp);
	paint_axis(p, pp, get_visual_y());
}

void DecodeTrace::paint_mid(QPainter &p, ViewItemPaintParams &pp)
{
	lock_guard<mutex> lock(row_modification_mutex_);

	// Set default pen to allow for text width calculation
	p.setPen(Qt::black);

	pair<uint64_t, uint64_t> sample_range = get_view_sample_range(pp.left(), pp.right());

	// Just because the view says we see a certain sample range it
	// doesn't mean we have this many decoded samples, too, so crop
	// the range to what has been decoded already
	sample_range.second = min((int64_t)sample_range.second,
		decode_signal_->get_decoded_sample_count(current_segment_, false));

	for (RowData& r : rows_)
		r.currently_visible = false;
	visible_rows_ = 0;
	int y = get_visual_y();

	for (RowData& r : rows_) {
		// If the row is hidden, we don't want to fetch annotations
		if ((!r.decode_row.decoder()->shown()) || (!r.decode_row.visible())) {
			r.currently_visible = false;
			continue;
		}

		vector<Annotation> annotations;
		decode_signal_->get_annotation_subset(annotations, r.decode_row,
			current_segment_, sample_range.first, sample_range.second);

		// Show row if there are visible annotations or when user wants to see
		// all rows that have annotations somewhere and this one is one of them
		size_t ann_count = decode_signal_->get_annotation_count(r.decode_row, current_segment_);
		r.currently_visible = !annotations.empty() || (always_show_all_rows_ && (ann_count > 0));

		if (r.currently_visible) {
			draw_annotations(annotations, p, annotation_height_, pp, y,
				get_row_color(r.decode_row.index()), r.title_width);
			y += r.height;
			visible_rows_++;
		}
	}

	draw_unresolved_period(p, annotation_height_, pp.left(), pp.right());

	if (visible_rows_ > max_visible_rows_) {
		max_visible_rows_ = visible_rows_;

		// Call order is important, otherwise the lazy event handler won't work
		owner_->extents_changed(false, true);
		owner_->row_item_appearance_changed(false, true);
	}

	const QString err = decode_signal_->error_message();
	if (!err.isEmpty())
		draw_error(p, err, pp);
}

void DecodeTrace::paint_fore(QPainter &p, ViewItemPaintParams &pp)
{
	unsigned int y = get_visual_y();

	for (const RowData& r : rows_) {
		if (!r.currently_visible)
			continue;

		p.setPen(QPen(Qt::NoPen));

		if (r.expand_marker_highlighted)
			p.setBrush(QApplication::palette().brush(QPalette::Highlight));
		else
			p.setBrush(QApplication::palette().brush(QPalette::WindowText));

		// Draw expansion marker
		QPolygon marker(r.expand_marker_shape);
		marker.translate(pp.left(), y);
		p.drawPolygon(marker);

		p.setBrush(QApplication::palette().brush(QPalette::WindowText));

		const QRect text_rect(pp.left() + ArrowSize * 2, y - r.height / 2,
			pp.right() - pp.left(), r.height);
		const QString h(r.decode_row.title());
		const int f = Qt::AlignLeft | Qt::AlignVCenter |
			Qt::TextDontClip;

		// Draw the outline
		p.setPen(QApplication::palette().color(QPalette::Base));
		for (int dx = -1; dx <= 1; dx++)
			for (int dy = -1; dy <= 1; dy++)
				if (dx != 0 && dy != 0)
					p.drawText(text_rect.translated(dx, dy), f, h);

		// Draw the text
		p.setPen(QApplication::palette().color(QPalette::WindowText));
		p.drawText(text_rect, f, h);

		y += r.height;
	}

	if (show_hover_marker_)
		paint_hover_marker(p);
}

void DecodeTrace::update_stack_button()
{
	const vector< shared_ptr<data::decode::Decoder> > &stack = decode_signal_->decoder_stack();

	// Only show decoders in the menu that can be stacked onto the last one in the stack
	if (!stack.empty()) {
		const srd_decoder* d = stack.back()->decoder();

		if (d->outputs) {
			pv::widgets::DecoderMenu *const decoder_menu =
				new pv::widgets::DecoderMenu(stack_button_, (const char*)(d->outputs->data));
			connect(decoder_menu, SIGNAL(decoder_selected(srd_decoder*)),
				this, SLOT(on_stack_decoder(srd_decoder*)));

			decoder_menu->setStyleSheet("QMenu { menu-scrollable: 1; }");

			stack_button_->setMenu(decoder_menu);
			stack_button_->show();
			return;
		}
	}

	// No decoders available for stacking
	stack_button_->setMenu(nullptr);
	stack_button_->hide();
}

void DecodeTrace::populate_popup_form(QWidget *parent, QFormLayout *form)
{
	using pv::data::decode::Decoder;

	assert(form);

	// Add the standard options
	Trace::populate_popup_form(parent, form);

	// Add the decoder options
	bindings_.clear();
	channel_id_map_.clear();
	init_state_map_.clear();
	decoder_forms_.clear();

	const vector< shared_ptr<Decoder> > &stack = decode_signal_->decoder_stack();

	if (stack.empty()) {
		QLabel *const l = new QLabel(
			tr("<p><i>No decoders in the stack</i></p>"));
		l->setAlignment(Qt::AlignCenter);
		form->addRow(l);
	} else {
		auto iter = stack.cbegin();
		for (int i = 0; i < (int)stack.size(); i++, iter++) {
			shared_ptr<Decoder> dec(*iter);
			create_decoder_form(i, dec, parent, form);
		}

		form->addRow(new QLabel(
			tr("<i>* Required channels</i>"), parent));
	}

	// Add stacking button
	stack_button_ = new QPushButton(tr("Stack Decoder"), parent);
	stack_button_->setToolTip(tr("Stack a higher-level decoder on top of this one"));
	update_stack_button();

	QHBoxLayout *stack_button_box = new QHBoxLayout;
	stack_button_box->addWidget(stack_button_, 0, Qt::AlignRight);
	form->addRow(stack_button_box);
}

QMenu* DecodeTrace::create_header_context_menu(QWidget *parent)
{
	QMenu *const menu = Trace::create_header_context_menu(parent);

	menu->addSeparator();

	QAction *const del = new QAction(tr("Delete"), this);
	del->setShortcuts(QKeySequence::Delete);
	connect(del, SIGNAL(triggered()), this, SLOT(on_delete()));
	menu->addAction(del);

	return menu;
}

QMenu* DecodeTrace::create_view_context_menu(QWidget *parent, QPoint &click_pos)
{
	// Get entries from default menu before adding our own
	QMenu *const menu = new QMenu(parent);

	QMenu* default_menu = Trace::create_view_context_menu(parent, click_pos);
	if (default_menu) {
		for (QAction *action : default_menu->actions()) {  // clazy:exclude=range-loop
			menu->addAction(action);
			if (action->parent() == default_menu)
				action->setParent(menu);
		}
		delete default_menu;

		// Add separator if needed
		if (menu->actions().length() > 0)
			menu->addSeparator();
	}

	selected_row_ = nullptr;
	const RowData* r = get_row_at_point(click_pos);
	if (r)
		selected_row_ = &(r->decode_row);

	const View *const view = owner_->view();
	assert(view);
	QPoint pos = view->viewport()->mapFrom(parent, click_pos);

	// Default sample range is "from here"
	const pair<uint64_t, uint64_t> sample_range = get_view_sample_range(pos.x(), pos.x() + 1);
	selected_sample_range_ = make_pair(sample_range.first, numeric_limits<uint64_t>::max());

	if (decode_signal_->is_paused()) {
		QAction *const resume =
			new QAction(tr("Resume decoding"), this);
		resume->setIcon(QIcon::fromTheme("media-playback-start",
			QIcon(":/icons/media-playback-start.png")));
		connect(resume, SIGNAL(triggered()), this, SLOT(on_pause_decode()));
		menu->addAction(resume);
	} else {
		QAction *const pause =
			new QAction(tr("Pause decoding"), this);
		pause->setIcon(QIcon::fromTheme("media-playback-pause",
			QIcon(":/icons/media-playback-pause.png")));
		connect(pause, SIGNAL(triggered()), this, SLOT(on_pause_decode()));
		menu->addAction(pause);
	}

	QAction *const copy_annotation_to_clipboard =
		new QAction(tr("Copy annotation text to clipboard"), this);
	copy_annotation_to_clipboard->setIcon(QIcon::fromTheme("edit-paste",
		QIcon(":/icons/edit-paste.svg")));
	connect(copy_annotation_to_clipboard, SIGNAL(triggered()), this, SLOT(on_copy_annotation_to_clipboard()));
	menu->addAction(copy_annotation_to_clipboard);

	menu->addSeparator();

	QAction *const export_all_rows =
		new QAction(tr("Export all annotations"), this);
	export_all_rows->setIcon(QIcon::fromTheme("document-save-as",
		QIcon(":/icons/document-save-as.png")));
	connect(export_all_rows, SIGNAL(triggered()), this, SLOT(on_export_all_rows()));
	menu->addAction(export_all_rows);

	QAction *const export_row =
		new QAction(tr("Export all annotations for this row"), this);
	export_row->setIcon(QIcon::fromTheme("document-save-as",
		QIcon(":/icons/document-save-as.png")));
	connect(export_row, SIGNAL(triggered()), this, SLOT(on_export_row()));
	menu->addAction(export_row);

	menu->addSeparator();

	QAction *const export_all_rows_from_here =
		new QAction(tr("Export all annotations, starting here"), this);
	export_all_rows_from_here->setIcon(QIcon::fromTheme("document-save-as",
		QIcon(":/icons/document-save-as.png")));
	connect(export_all_rows_from_here, SIGNAL(triggered()), this, SLOT(on_export_all_rows_from_here()));
	menu->addAction(export_all_rows_from_here);

	QAction *const export_row_from_here =
		new QAction(tr("Export annotations for this row, starting here"), this);
	export_row_from_here->setIcon(QIcon::fromTheme("document-save-as",
		QIcon(":/icons/document-save-as.png")));
	connect(export_row_from_here, SIGNAL(triggered()), this, SLOT(on_export_row_from_here()));
	menu->addAction(export_row_from_here);

	menu->addSeparator();

	QAction *const export_all_rows_with_cursor =
		new QAction(tr("Export all annotations within cursor range"), this);
	export_all_rows_with_cursor->setIcon(QIcon::fromTheme("document-save-as",
		QIcon(":/icons/document-save-as.png")));
	connect(export_all_rows_with_cursor, SIGNAL(triggered()), this, SLOT(on_export_all_rows_with_cursor()));
	menu->addAction(export_all_rows_with_cursor);

	QAction *const export_row_with_cursor =
		new QAction(tr("Export annotations for this row within cursor range"), this);
	export_row_with_cursor->setIcon(QIcon::fromTheme("document-save-as",
		QIcon(":/icons/document-save-as.png")));
	connect(export_row_with_cursor, SIGNAL(triggered()), this, SLOT(on_export_row_with_cursor()));
	menu->addAction(export_row_with_cursor);

	if (!view->cursors()->enabled()) {
		export_all_rows_with_cursor->setEnabled(false);
		export_row_with_cursor->setEnabled(false);
	}

	return menu;
}

void DecodeTrace::delete_pressed()
{
	on_delete();
}

void DecodeTrace::hover_point_changed(const QPoint &hp)
{
	Trace::hover_point_changed(hp);

	assert(owner_);

	RowData* hover_row = get_row_at_point(hp);

	// Row expansion marker handling
	for (RowData& r : rows_)
		r.expand_marker_highlighted = false;

	if (hover_row) {
		int row_y = get_row_y(hover_row);
		if ((hp.x() > 0) && (hp.x() < 2 * ArrowSize) &&
			(hp.y() > (int)(row_y - ArrowSize)) && (hp.y() < (int)(row_y + ArrowSize)))
			hover_row->expand_marker_highlighted = true;
	}

	// Tooltip handling
	if (hp.x() > 0) {
		QString ann = get_annotation_at_point(hp);

		if (!ann.isEmpty()) {
			QFontMetrics m(QToolTip::font());
			const QRect text_size = m.boundingRect(QRect(), 0, ann);

			// This is OS-specific and unfortunately we can't query it, so
			// use an approximation to at least try to minimize the error.
			const int padding = default_row_height_ + 8;

			// Make sure the tool tip doesn't overlap with the mouse cursor.
			// If it did, the tool tip would constantly hide and re-appear.
			// We also push it up by one row so that it appears above the
			// decode trace, not below.
			QPoint p = hp;
			p.setX(hp.x() - (text_size.width() / 2) - padding);

			p.setY(get_row_y(hover_row) - default_row_height_ -
				text_size.height() - padding);

			const View *const view = owner_->view();
			assert(view);
			QToolTip::showText(view->viewport()->mapToGlobal(p), ann);

		} else
			QToolTip::hideText();

	} else
		QToolTip::hideText();
}

void DecodeTrace::mouse_left_press_event(const QMouseEvent* event)
{
	// Handle row expansion marker
	for (RowData& r : rows_) {
		if (!r.expand_marker_highlighted)
			continue;

		unsigned int y = get_row_y(&r);
		if ((event->x() > 0) && (event->x() <= (int)(ArrowSize + 3)) &&
			(event->y() > (int)(y - (default_row_height_ / 2))) &&
			(event->y() <= (int)(y + (default_row_height_ / 2)))) {

			if (r.expanded) {
				r.collapsing = true;
				r.expanded = false;
				r.anim_shape = ArrowSize;
			} else {
				r.expanding = true;
				r.anim_shape = 0;
				r.container->setVisible(true);
				QApplication::processEvents();
				r.expanded_height = 5 * default_row_height_ + r.container->size().height();
			}

			r.animation_step = 0;
			r.anim_height = r.height;

			update_expanded_rows();
			animation_timer_.start();
		}
	}
}

void DecodeTrace::draw_annotations(vector<pv::data::decode::Annotation> annotations,
		QPainter &p, int h, const ViewItemPaintParams &pp, int y,
		QColor row_color, int row_title_width)
{
	using namespace pv::data::decode;

	Annotation::Class block_class = 0;
	bool block_class_uniform = true;
	qreal block_start = 0;
	int block_ann_count = 0;

	const Annotation *prev_ann;
	qreal prev_end = INT_MIN;

	qreal a_end;

	double samples_per_pixel, pixels_offset;
	tie(pixels_offset, samples_per_pixel) =
		get_pixels_offset_samples_per_pixel();

	// Sort the annotations by start sample so that decoders
	// can't confuse us by creating annotations out of order
	stable_sort(annotations.begin(), annotations.end(),
		[](const Annotation &a, const Annotation &b) {
			return a.start_sample() < b.start_sample(); });

	// Gather all annotations that form a visual "block" and draw them as such
	for (const Annotation &a : annotations) {

		const qreal abs_a_start = a.start_sample() / samples_per_pixel;
		const qreal abs_a_end   = a.end_sample() / samples_per_pixel;

		const qreal a_start = abs_a_start - pixels_offset;
		a_end = abs_a_end - pixels_offset;

		const qreal a_width = a_end - a_start;
		const qreal delta = a_end - prev_end;

		bool a_is_separate = false;

		// Annotation wider than the threshold for a useful label width?
		if (a_width >= min_useful_label_width_) {
			for (const QString &ann_text : a.annotations()) {
				const qreal w = p.boundingRect(QRectF(), 0, ann_text).width();
				// Annotation wide enough to fit a label? Don't put it in a block then
				if (w <= a_width) {
					a_is_separate = true;
					break;
				}
			}
		}

		// Were the previous and this annotation more than a pixel apart?
		if ((abs(delta) > 1) || a_is_separate) {
			// Block was broken, draw annotations that form the current block
			if (block_ann_count == 1)
				draw_annotation(*prev_ann, p, h, pp, y, row_color,
					row_title_width);
			else if (block_ann_count > 0)
				draw_annotation_block(block_start, prev_end, block_class,
					block_class_uniform, p, h, y, row_color);

			block_ann_count = 0;
		}

		if (a_is_separate) {
			draw_annotation(a, p, h, pp, y, row_color, row_title_width);
			// Next annotation must start a new block. delta will be > 1
			// because we set prev_end to INT_MIN but that's okay since
			// block_ann_count will be 0 and nothing will be drawn
			prev_end = INT_MIN;
			block_ann_count = 0;
		} else {
			prev_end = a_end;
			prev_ann = &a;

			if (block_ann_count == 0) {
				block_start = a_start;
				block_class = a.ann_class();
				block_class_uniform = true;
			} else
				if (a.ann_class() != block_class)
					block_class_uniform = false;

			block_ann_count++;
		}
	}

	if (block_ann_count == 1)
		draw_annotation(*prev_ann, p, h, pp, y, row_color, row_title_width);
	else if (block_ann_count > 0)
		draw_annotation_block(block_start, prev_end, block_class,
			block_class_uniform, p, h, y, row_color);
}

void DecodeTrace::draw_annotation(const pv::data::decode::Annotation &a,
	QPainter &p, int h, const ViewItemPaintParams &pp, int y,
	QColor row_color, int row_title_width) const
{
	double samples_per_pixel, pixels_offset;
	tie(pixels_offset, samples_per_pixel) =
		get_pixels_offset_samples_per_pixel();

	const double start = a.start_sample() / samples_per_pixel -
		pixels_offset;
	const double end = a.end_sample() / samples_per_pixel - pixels_offset;

	QColor color = get_annotation_color(row_color, a.ann_class());
	p.setPen(color.darker());
	p.setBrush(color);

	if (start > pp.right() + DrawPadding || end < pp.left() - DrawPadding)
		return;

	if (a.start_sample() == a.end_sample())
		draw_instant(a, p, h, start, y);
	else
		draw_range(a, p, h, start, end, y, pp, row_title_width);
}

void DecodeTrace::draw_annotation_block(qreal start, qreal end,
	Annotation::Class ann_class, bool use_ann_format, QPainter &p, int h,
	int y, QColor row_color) const
{
	const double top = y + .5 - h / 2;
	const double bottom = y + .5 + h / 2;

	const QRectF rect(start, top, end - start, bottom - top);
	const int r = h / 4;

	p.setPen(QPen(Qt::NoPen));
	p.setBrush(Qt::white);
	p.drawRoundedRect(rect, r, r);

	// If all annotations in this block are of the same type, we can use the
	// one format that all of these annotations have. Otherwise, we should use
	// a neutral color (i.e. gray)
	if (use_ann_format) {
		const QColor color = get_annotation_color(row_color, ann_class);
		p.setPen(color.darker());
		p.setBrush(QBrush(color, Qt::Dense4Pattern));
	} else {
		p.setPen(Qt::gray);
		p.setBrush(QBrush(Qt::gray, Qt::Dense4Pattern));
	}

	p.drawRoundedRect(rect, r, r);
}

void DecodeTrace::draw_instant(const pv::data::decode::Annotation &a, QPainter &p,
	int h, qreal x, int y) const
{
	const QString text = a.annotations().empty() ?
		QString() : a.annotations().back();
	const qreal w = min((qreal)p.boundingRect(QRectF(), 0, text).width(),
		0.0) + h;
	const QRectF rect(x - w / 2, y - h / 2, w, h);

	p.drawRoundedRect(rect, h / 2, h / 2);

	p.setPen(Qt::black);
	p.drawText(rect, Qt::AlignCenter | Qt::AlignVCenter, text);
}

void DecodeTrace::draw_range(const pv::data::decode::Annotation &a, QPainter &p,
	int h, qreal start, qreal end, int y, const ViewItemPaintParams &pp,
	int row_title_width) const
{
	const qreal top = y + .5 - h / 2;
	const qreal bottom = y + .5 + h / 2;
	const vector<QString> annotations = a.annotations();

	// If the two ends are within 1 pixel, draw a vertical line
	if (start + 1.0 > end) {
		p.drawLine(QPointF(start, top), QPointF(start, bottom));
		return;
	}

	const qreal cap_width = min((end - start) / 4, EndCapWidth);

	QPointF pts[] = {
		QPointF(start, y + .5f),
		QPointF(start + cap_width, top),
		QPointF(end - cap_width, top),
		QPointF(end, y + .5f),
		QPointF(end - cap_width, bottom),
		QPointF(start + cap_width, bottom)
	};

	p.drawConvexPolygon(pts, countof(pts));

	if (annotations.empty())
		return;

	const int ann_start = start + cap_width;
	const int ann_end = end - cap_width;

	const int real_start = max(ann_start, pp.left() + ArrowSize + row_title_width);
	const int real_end = min(ann_end, pp.right());
	const int real_width = real_end - real_start;

	QRectF rect(real_start, y - h / 2, real_width, h);
	if (rect.width() <= 4)
		return;

	p.setPen(Qt::black);

	// Try to find an annotation that will fit
	QString best_annotation;
	int best_width = 0;

	for (const QString &a : annotations) {
		const int w = p.boundingRect(QRectF(), 0, a).width();
		if (w <= rect.width() && w > best_width)
			best_annotation = a, best_width = w;
	}

	if (best_annotation.isEmpty())
		best_annotation = annotations.back();

	// If not ellide the last in the list
	p.drawText(rect, Qt::AlignCenter, p.fontMetrics().elidedText(
		best_annotation, Qt::ElideRight, rect.width()));
}

void DecodeTrace::draw_error(QPainter &p, const QString &message,
	const ViewItemPaintParams &pp)
{
	const int y = get_visual_y();

	double samples_per_pixel, pixels_offset;
	tie(pixels_offset, samples_per_pixel) = get_pixels_offset_samples_per_pixel();

	p.setPen(ErrorBgColor.darker());
	p.setBrush(ErrorBgColor);

	const QRectF bounding_rect = QRectF(pp.left(), INT_MIN / 2 + y, pp.right(), INT_MAX);

	const QRectF text_rect = p.boundingRect(bounding_rect, Qt::AlignCenter, message);
	const qreal r = text_rect.height() / 4;

	p.drawRoundedRect(text_rect.adjusted(-r, -r, r, r), r, r, Qt::AbsoluteSize);

	p.setPen(Qt::black);
	p.drawText(text_rect, message);
}

void DecodeTrace::draw_unresolved_period(QPainter &p, int h, int left, int right) const
{
	using namespace pv::data;
	using pv::data::decode::Decoder;

	double samples_per_pixel, pixels_offset;

	const int64_t sample_count = decode_signal_->get_working_sample_count(current_segment_);
	if (sample_count == 0)
		return;

	const int64_t samples_decoded = decode_signal_->get_decoded_sample_count(current_segment_, true);
	if (sample_count == samples_decoded)
		return;

	const int y = get_visual_y();

	tie(pixels_offset, samples_per_pixel) = get_pixels_offset_samples_per_pixel();

	const double start = max(samples_decoded /
		samples_per_pixel - pixels_offset, left - 1.0);
	const double end = min(sample_count / samples_per_pixel -
		pixels_offset, right + 1.0);
	const QRectF no_decode_rect(start, y - (h / 2) - 0.5, end - start, h);

	p.setPen(QPen(Qt::NoPen));
	p.setBrush(Qt::white);
	p.drawRect(no_decode_rect);

	p.setPen(NoDecodeColor);
	p.setBrush(QBrush(NoDecodeColor, Qt::Dense6Pattern));
	p.drawRect(no_decode_rect);
}

pair<double, double> DecodeTrace::get_pixels_offset_samples_per_pixel() const
{
	assert(owner_);

	const View *view = owner_->view();
	assert(view);

	const double scale = view->scale();
	assert(scale > 0);

	const double pixels_offset =
		((view->offset() - decode_signal_->start_time()) / scale).convert_to<double>();

	double samplerate = decode_signal_->samplerate();

	// Show sample rate as 1Hz when it is unknown
	if (samplerate == 0.0)
		samplerate = 1.0;

	return make_pair(pixels_offset, samplerate * scale);
}

pair<uint64_t, uint64_t> DecodeTrace::get_view_sample_range(
	int x_start, int x_end) const
{
	double samples_per_pixel, pixels_offset;
	tie(pixels_offset, samples_per_pixel) =
		get_pixels_offset_samples_per_pixel();

	const uint64_t start = (uint64_t)max(
		(x_start + pixels_offset) * samples_per_pixel, 0.0);
	const uint64_t end = (uint64_t)max(
		(x_end + pixels_offset) * samples_per_pixel, 0.0);

	return make_pair(start, end);
}

QColor DecodeTrace::get_row_color(int row_index) const
{
	// For each row color, use the base color hue and add an offset that's
	// not a dividend of 360

	QColor color;
	const int h = (base_->color().toHsv().hue() + 20 * row_index) % 360;
	const int s = DECODETRACE_COLOR_SATURATION;
	const int v = DECODETRACE_COLOR_VALUE;
	color.setHsl(h, s, v);

	return color;
}

QColor DecodeTrace::get_annotation_color(QColor row_color, int annotation_index) const
{
	// For each row color, use the base color hue and add an offset that's
	// not a dividend of 360 and not a multiple of the row offset

	QColor color(row_color);
	const int h = (color.toHsv().hue() + 55 * annotation_index) % 360;
	const int s = DECODETRACE_COLOR_SATURATION;
	const int v = DECODETRACE_COLOR_VALUE;
	color.setHsl(h, s, v);

	return color;
}

unsigned int DecodeTrace::get_row_y(const RowData* row) const
{
	assert(row);

	unsigned int y = get_visual_y();

	for (const RowData& r : rows_) {
		if (!r.currently_visible)
			continue;

		if (row->decode_row == r.decode_row)
			break;
		else
			y += r.height;
	}

	return y;
}

RowData* DecodeTrace::get_row_at_point(const QPoint &point)
{
	int y = get_visual_y() - (default_row_height_ / 2);

	for (RowData& r : rows_) {
		if (!r.currently_visible)
			continue;

		if ((point.y() >= y) && (point.y() < (int)(y + r.height)))
			return &r;

		y += r.height;
	}

	return nullptr;
}

const QString DecodeTrace::get_annotation_at_point(const QPoint &point)
{
	using namespace pv::data::decode;

	if (!enabled())
		return QString();

	const pair<uint64_t, uint64_t> sample_range =
		get_view_sample_range(point.x(), point.x() + 1);
	const RowData* r = get_row_at_point(point);

	if (!r)
		return QString();

	if (point.y() > (int)(get_row_y(r) + (annotation_height_ / 2)))
		return QString();

	vector<Annotation> annotations;

	decode_signal_->get_annotation_subset(annotations, r->decode_row,
		current_segment_, sample_range.first, sample_range.second);

	return (annotations.empty()) ?
		QString() : annotations[0].annotations().front();
}

void DecodeTrace::create_decoder_form(int index,
	shared_ptr<data::decode::Decoder> &dec, QWidget *parent,
	QFormLayout *form)
{
	GlobalSettings settings;

	assert(dec);
	const srd_decoder *const decoder = dec->decoder();
	assert(decoder);

	const bool decoder_deletable = index > 0;

	pv::widgets::DecoderGroupBox *const group =
		new pv::widgets::DecoderGroupBox(
			QString::fromUtf8(decoder->name),
			tr("%1:\n%2").arg(QString::fromUtf8(decoder->longname),
				QString::fromUtf8(decoder->desc)),
			nullptr, decoder_deletable);
	group->set_decoder_visible(dec->shown());

	if (decoder_deletable) {
		delete_mapper_.setMapping(group, index);
		connect(group, SIGNAL(delete_decoder()), &delete_mapper_, SLOT(map()));
	}

	show_hide_mapper_.setMapping(group, index);
	connect(group, SIGNAL(show_hide_decoder()),
		&show_hide_mapper_, SLOT(map()));

	QFormLayout *const decoder_form = new QFormLayout;
	group->add_layout(decoder_form);

	const vector<DecodeChannel> channels = decode_signal_->get_channels();

	// Add the channels
	for (const DecodeChannel& ch : channels) {
		// Ignore channels not part of the decoder we create the form for
		if (ch.decoder_ != dec)
			continue;

		QComboBox *const combo = create_channel_selector(parent, &ch);
		QComboBox *const combo_init_state = create_channel_selector_init_state(parent, &ch);

		channel_id_map_[combo] = ch.id;
		init_state_map_[combo_init_state] = ch.id;

		connect(combo, SIGNAL(currentIndexChanged(int)),
			this, SLOT(on_channel_selected(int)));
		connect(combo_init_state, SIGNAL(currentIndexChanged(int)),
			this, SLOT(on_init_state_changed(int)));

		QHBoxLayout *const hlayout = new QHBoxLayout;
		hlayout->addWidget(combo);
		hlayout->addWidget(combo_init_state);

		if (!settings.value(GlobalSettings::Key_Dec_InitialStateConfigurable).toBool())
			combo_init_state->hide();

		const QString required_flag = ch.is_optional ? QString() : QString("*");
		decoder_form->addRow(tr("<b>%1</b> (%2) %3")
			.arg(ch.name, ch.desc, required_flag), hlayout);
	}

	// Add the options
	shared_ptr<binding::Decoder> binding(
		new binding::Decoder(decode_signal_, dec));
	binding->add_properties_to_form(decoder_form, true);

	bindings_.push_back(binding);

	form->addRow(group);
	decoder_forms_.push_back(group);
}

QComboBox* DecodeTrace::create_channel_selector(QWidget *parent, const DecodeChannel *ch)
{
	const auto sigs(session_.signalbases());

	// Sort signals in natural order
	vector< shared_ptr<data::SignalBase> > sig_list(sigs.begin(), sigs.end());
	sort(sig_list.begin(), sig_list.end(),
		[](const shared_ptr<data::SignalBase> &a,
		const shared_ptr<data::SignalBase> &b) {
			return strnatcasecmp(a->name().toStdString(),
				b->name().toStdString()) < 0; });

	QComboBox *selector = new QComboBox(parent);

	selector->addItem("-", qVariantFromValue((void*)nullptr));

	if (!ch->assigned_signal)
		selector->setCurrentIndex(0);

	for (const shared_ptr<data::SignalBase> &b : sig_list) {
		assert(b);
		if (b->logic_data() && b->enabled()) {
			selector->addItem(b->name(),
				qVariantFromValue((void*)b.get()));

			if (ch->assigned_signal == b.get())
				selector->setCurrentIndex(selector->count() - 1);
		}
	}

	return selector;
}

QComboBox* DecodeTrace::create_channel_selector_init_state(QWidget *parent,
	const DecodeChannel *ch)
{
	QComboBox *selector = new QComboBox(parent);

	selector->addItem("0", qVariantFromValue((int)SRD_INITIAL_PIN_LOW));
	selector->addItem("1", qVariantFromValue((int)SRD_INITIAL_PIN_HIGH));
	selector->addItem("X", qVariantFromValue((int)SRD_INITIAL_PIN_SAME_AS_SAMPLE0));

	selector->setCurrentIndex(ch->initial_pin_state);

	selector->setToolTip("Initial (assumed) pin value before the first sample");

	return selector;
}

void DecodeTrace::export_annotations(vector<Annotation> *annotations) const
{
	using namespace pv::data::decode;

	GlobalSettings settings;
	const QString dir = settings.value("MainWindow/SaveDirectory").toString();

	const QString file_name = QFileDialog::getSaveFileName(
		owner_->view(), tr("Export annotations"), dir, tr("Text Files (*.txt);;All Files (*)"));

	if (file_name.isEmpty())
		return;

	QString format = settings.value(GlobalSettings::Key_Dec_ExportFormat).toString();
	const QString quote = format.contains("%q") ? "\"" : "";
	format = format.remove("%q");

	QFile file(file_name);
	if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
		QTextStream out_stream(&file);

		for (Annotation &ann : *annotations) {
			const QString sample_range = QString("%1-%2") \
				.arg(QString::number(ann.start_sample()), QString::number(ann.end_sample()));

			const QString class_name = quote + ann.row()->class_name() + quote;

			QString all_ann_text;
			for (const QString &s : ann.annotations())
				all_ann_text = all_ann_text + quote + s + quote + ",";
			all_ann_text.chop(1);

			const QString first_ann_text = quote + ann.annotations().front() + quote;

			QString out_text = format;
			out_text = out_text.replace("%s", sample_range);
			out_text = out_text.replace("%d",
				quote + QString::fromUtf8(ann.row()->decoder()->name()) + quote);
			out_text = out_text.replace("%c", class_name);
			out_text = out_text.replace("%1", first_ann_text);
			out_text = out_text.replace("%a", all_ann_text);
			out_stream << out_text << '\n';
		}

		if (out_stream.status() == QTextStream::Ok)
			return;
	}

	QMessageBox msg(owner_->view());
	msg.setText(tr("Error") + "\n\n" + tr("File %1 could not be written to.").arg(file_name));
	msg.setStandardButtons(QMessageBox::Ok);
	msg.setIcon(QMessageBox::Warning);
	msg.exec();
}

void DecodeTrace::update_rows()
{
	lock_guard<mutex> lock(row_modification_mutex_);

	QFontMetrics m(QApplication::font());

	QPalette header_palette = owner_->view()->palette();
	QPalette selector_palette = owner_->view()->palette();

	if (GlobalSettings::current_theme_is_dark()) {
		header_palette.setColor(QPalette::Background,
			QColor(255, 255, 255, ExpansionAreaHeaderAlpha));
		selector_palette.setColor(QPalette::Background,
			QColor(255, 255, 255, ExpansionAreaAlpha));
	} else {
		header_palette.setColor(QPalette::Background,
			QColor(0, 0, 0, ExpansionAreaHeaderAlpha));
		selector_palette.setColor(QPalette::Background,
			QColor(0, 0, 0, ExpansionAreaAlpha));
	}

	for (RowData& r : rows_)
		r.exists = false;

	unsigned int row_id = 0;
	for (const Row& decode_row : decode_signal_->get_rows()) {
		// Find row in our list
		auto r_it = find_if(rows_.begin(), rows_.end(),
			[&](RowData& r){ return r.decode_row == decode_row; });

		RowData* r = nullptr;
		if (r_it == rows_.end()) {
			// Row doesn't exist yet, create and append it
			RowData nr;
			nr.decode_row = decode_row;
			nr.height = default_row_height_;
			nr.expanded_height = default_row_height_;
			nr.currently_visible = false;
			nr.expand_marker_highlighted = false;
			nr.expanding = false;
			nr.expanded = false;
			nr.collapsing = false;
			nr.expand_marker_shape = default_marker_shape_;
			nr.container = new QWidget(owner_->view()->scrollarea());
			nr.header_container = new QWidget(nr.container);
			nr.selector_container = new QWidget(nr.container);

			rows_.push_back(nr);
			r = &rows_.back();
		} else
			r = &(*r_it);

		r->exists = true;

		const int w = m.boundingRect(r->decode_row.title()).width() + RowTitleMargin;
		r->title_width = w;

		r->container->resize(owner_->view()->viewport()->width() - r->container->pos().x(),
			r->expanded_height - 2 * default_row_height_);
		r->container->setVisible(false);

		QVBoxLayout* vlayout = new QVBoxLayout();
		r->container->setLayout(vlayout);

		// Add header container with checkbox for this row
		vlayout->addWidget(r->header_container);
		vlayout->setContentsMargins(0, 0, 0, 0);
		vlayout->setSpacing(0);
		r->header_container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		r->header_container->setMinimumSize(0, default_row_height_);
		r->header_container->setLayout(new QVBoxLayout());
		r->header_container->layout()->setContentsMargins(10, 2, 0, 2);

		r->header_container->setAutoFillBackground(true);
		r->header_container->setPalette(header_palette);

		QCheckBox* cb = new QCheckBox();
		r->header_container->layout()->addWidget(cb);
		cb->setText(tr("Show this row"));
		cb->setChecked(r->decode_row.visible());

		row_show_hide_mapper_.setMapping(cb, row_id);
		connect(cb, SIGNAL(stateChanged(int)),
			&row_show_hide_mapper_, SLOT(map()));

		// Add selector container
		vlayout->addWidget(r->selector_container);
		r->selector_container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		r->selector_container->setMinimumSize(0, 3 * default_row_height_);                            // FIXME
		r->selector_container->setLayout(new QVBoxLayout());

		r->selector_container->setAutoFillBackground(true);
		r->selector_container->setPalette(selector_palette);


		// Add all classes that can be toggled

		row_id++;
	}

	// Remove any rows that no longer exist, obeying that iterators are invalidated
	bool any_exists;
	do {
		any_exists = false;

		for (unsigned int i = 0; i < rows_.size(); i++)
			if (!rows_[i].exists) {
				for (QCheckBox* cb : rows_[i].selectors)
					delete cb;

				delete rows_[i].selector_container;
				delete rows_[i].header_container;
				delete rows_[i].container;

				rows_.erase(rows_.begin() + i);
				any_exists = true;
				break;
			}
	} while (any_exists);
}

void DecodeTrace::set_row_expanded(RowData* r)
{
	r->height = r->expanded_height;
	r->expanding = false;
	r->expanded = true;

	// For details on this, see on_animation_timer()
	r->expand_marker_shape.setPoint(0, 0, 0);
	r->expand_marker_shape.setPoint(1, ArrowSize, ArrowSize);
	r->expand_marker_shape.setPoint(2, 2*ArrowSize, 0);

	r->container->resize(owner_->view()->viewport()->width() - r->container->pos().x(),
		r->height - 2 * default_row_height_);
}

void DecodeTrace::set_row_collapsed(RowData* r)
{
	r->height = default_row_height_;
	r->collapsing = false;
	r->expanded = false;
	r->expand_marker_shape = default_marker_shape_;
	r->container->setVisible(false);

	r->container->resize(owner_->view()->viewport()->width() - r->container->pos().x(),
		r->height - 2 * default_row_height_);
}

void DecodeTrace::update_expanded_rows()
{
	for (RowData& r : rows_) {

		r.container->move(2 * ArrowSize,
			get_row_y(&r) + default_row_height_);
	}
}

void DecodeTrace::on_setting_changed(const QString &key, const QVariant &value)
{
	Trace::on_setting_changed(key, value);

	if (key == GlobalSettings::Key_Dec_AlwaysShowAllRows) {
		max_visible_rows_ = 0;
		always_show_all_rows_ = value.toBool();
	}
}

void DecodeTrace::on_new_annotations()
{
	if (!delayed_trace_updater_.isActive())
		delayed_trace_updater_.start();
}

void DecodeTrace::on_delayed_trace_update()
{
	if (owner_)
		owner_->row_item_appearance_changed(false, true);
}

void DecodeTrace::on_decode_reset()
{
	max_visible_rows_ = 0;
	update_rows();

	if (owner_)
		owner_->row_item_appearance_changed(false, true);
}

void DecodeTrace::on_decode_finished()
{
	if (owner_)
		owner_->row_item_appearance_changed(false, true);
}

void DecodeTrace::on_pause_decode()
{
	if (decode_signal_->is_paused())
		decode_signal_->resume_decode();
	else
		decode_signal_->pause_decode();
}

void DecodeTrace::on_delete()
{
	session_.remove_decode_signal(decode_signal_);
}

void DecodeTrace::on_channel_selected(int)
{
	QComboBox *cb = qobject_cast<QComboBox*>(QObject::sender());

	// Determine signal that was selected
	const data::SignalBase *signal =
		(data::SignalBase*)cb->itemData(cb->currentIndex()).value<void*>();

	// Determine decode channel ID this combo box is the channel selector for
	const uint16_t id = channel_id_map_.at(cb);

	decode_signal_->assign_signal(id, signal);
}

void DecodeTrace::on_channels_updated()
{
	if (owner_)
		owner_->row_item_appearance_changed(false, true);
}

void DecodeTrace::on_init_state_changed(int)
{
	QComboBox *cb = qobject_cast<QComboBox*>(QObject::sender());

	// Determine inital pin state that was selected
	int init_state = cb->itemData(cb->currentIndex()).value<int>();

	// Determine decode channel ID this combo box is the channel selector for
	const uint16_t id = init_state_map_.at(cb);

	decode_signal_->set_initial_pin_state(id, init_state);
}

void DecodeTrace::on_stack_decoder(srd_decoder *decoder)
{
	decode_signal_->stack_decoder(decoder);
	update_rows();

	create_popup_form();
}

void DecodeTrace::on_delete_decoder(int index)
{
	decode_signal_->remove_decoder(index);
	update_rows();

	// Force re-calculation of the trace height
	max_visible_rows_ = 0;
	owner_->extents_changed(false, true);

	create_popup_form();
}

void DecodeTrace::on_show_hide_decoder(int index)
{
	const bool state = decode_signal_->toggle_decoder_visibility(index);

	assert(index < (int)decoder_forms_.size());
	decoder_forms_[index]->set_decoder_visible(state);

	if (!state) {
		// Force re-calculation of the trace height, see paint_mid()
		max_visible_rows_ = 0;
		owner_->extents_changed(false, true);
	}

	owner_->row_item_appearance_changed(false, true);
}

void DecodeTrace::on_show_hide_row(int index)
{
	if (index >= (int)rows_.size())
		return;

	set_row_collapsed(&rows_[index]);
	rows_[index].decode_row.set_visible(!rows_[index].decode_row.visible());

	// Force re-calculation of the trace height, see paint_mid()
	max_visible_rows_ = 0;
	owner_->extents_changed(false, true);
	owner_->row_item_appearance_changed(false, true);
}

void DecodeTrace::on_copy_annotation_to_clipboard()
{
	using namespace pv::data::decode;

	if (!selected_row_)
		return;

	vector<Annotation> *annotations = new vector<Annotation>();

	decode_signal_->get_annotation_subset(*annotations, *selected_row_,
		current_segment_, selected_sample_range_.first, selected_sample_range_.first);

	if (annotations->empty())
		return;

	QClipboard *clipboard = QApplication::clipboard();
	clipboard->setText(annotations->front().annotations().front(), QClipboard::Clipboard);

	if (clipboard->supportsSelection())
		clipboard->setText(annotations->front().annotations().front(), QClipboard::Selection);

	delete annotations;
}

void DecodeTrace::on_export_row()
{
	selected_sample_range_ = make_pair(0, numeric_limits<uint64_t>::max());
	on_export_row_from_here();
}

void DecodeTrace::on_export_all_rows()
{
	selected_sample_range_ = make_pair(0, numeric_limits<uint64_t>::max());
	on_export_all_rows_from_here();
}

void DecodeTrace::on_export_row_with_cursor()
{
	const View *view = owner_->view();
	assert(view);

	if (!view->cursors()->enabled())
		return;

	const double samplerate = session_.get_samplerate();

	const pv::util::Timestamp& start_time = view->cursors()->first()->time();
	const pv::util::Timestamp& end_time = view->cursors()->second()->time();

	const uint64_t start_sample = (uint64_t)max(
		0.0, start_time.convert_to<double>() * samplerate);
	const uint64_t end_sample = (uint64_t)max(
		0.0, end_time.convert_to<double>() * samplerate);

	// Are both cursors negative and thus were clamped to 0?
	if ((start_sample == 0) && (end_sample == 0))
		return;

	selected_sample_range_ = make_pair(start_sample, end_sample);
	on_export_row_from_here();
}

void DecodeTrace::on_export_all_rows_with_cursor()
{
	const View *view = owner_->view();
	assert(view);

	if (!view->cursors()->enabled())
		return;

	const double samplerate = session_.get_samplerate();

	const pv::util::Timestamp& start_time = view->cursors()->first()->time();
	const pv::util::Timestamp& end_time = view->cursors()->second()->time();

	const uint64_t start_sample = (uint64_t)max(
		0.0, start_time.convert_to<double>() * samplerate);
	const uint64_t end_sample = (uint64_t)max(
		0.0, end_time.convert_to<double>() * samplerate);

	// Are both cursors negative and thus were clamped to 0?
	if ((start_sample == 0) && (end_sample == 0))
		return;

	selected_sample_range_ = make_pair(start_sample, end_sample);
	on_export_all_rows_from_here();
}

void DecodeTrace::on_export_row_from_here()
{
	using namespace pv::data::decode;

	if (!selected_row_)
		return;

	vector<Annotation> *annotations = new vector<Annotation>();

	decode_signal_->get_annotation_subset(*annotations, *selected_row_,
		current_segment_, selected_sample_range_.first, selected_sample_range_.second);

	if (annotations->empty())
		return;

	export_annotations(annotations);
	delete annotations;
}

void DecodeTrace::on_export_all_rows_from_here()
{
	using namespace pv::data::decode;

	vector<Annotation> *annotations = new vector<Annotation>();

	decode_signal_->get_annotation_subset(*annotations, current_segment_,
			selected_sample_range_.first, selected_sample_range_.second);

	if (!annotations->empty())
		export_annotations(annotations);

	delete annotations;
}

void DecodeTrace::on_animation_timer()
{
	bool animation_finished = true;

	for (RowData& r : rows_) {
		if (!(r.expanding || r.collapsing))
			continue;

		unsigned int height_delta = r.expanded_height - default_row_height_;

		if (r.expanding) {
			if (r.height < r.expanded_height) {
				r.anim_height += height_delta / (float)AnimationDurationInTicks;
				r.height = r.anim_height;
				r.anim_shape += ArrowSize / (float)AnimationDurationInTicks;
				animation_finished = false;
			} else
				set_row_expanded(&r);
		}

		if (r.collapsing) {
			if (r.height > default_row_height_) {
				r.anim_height -= height_delta / (float)AnimationDurationInTicks;
				r.height = r.anim_height;
				r.anim_shape -= ArrowSize / (float)AnimationDurationInTicks;
				animation_finished = false;
			} else
				set_row_collapsed(&r);
		}

		// The expansion marker shape switches between
		// 0/-A, A/0,  0/A (default state; anim_shape=0) and
		// 0/ 0, A/A, 2A/0 (expanded state; anim_shape=ArrowSize)

		r.expand_marker_shape.setPoint(0, 0, -ArrowSize + r.anim_shape);
		r.expand_marker_shape.setPoint(1, ArrowSize, r.anim_shape);
		r.expand_marker_shape.setPoint(2, 2*r.anim_shape, ArrowSize - r.anim_shape);
	}

	if (animation_finished)
		animation_timer_.stop();

	owner_->extents_changed(false, true);
	owner_->row_item_appearance_changed(false, true);
}

} // namespace trace
} // namespace views
} // namespace pv
