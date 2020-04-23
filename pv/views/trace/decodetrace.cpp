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
#include <QDebug>
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
#include <pv/widgets/flowlayout.hpp>

using std::abs;
using std::find_if;
using std::lock_guard;
using std::make_pair;
using std::max;
using std::min;
using std::numeric_limits;
using std::pair;
using std::shared_ptr;
using std::tie;
using std::vector;

using pv::data::decode::Annotation;
using pv::data::decode::AnnotationClass;
using pv::data::decode::Row;
using pv::data::decode::DecodeChannel;
using pv::data::DecodeSignal;

namespace pv {
namespace views {
namespace trace {

const QColor DecodeTrace::ErrorBgColor = QColor(0xEF, 0x29, 0x29);
const QColor DecodeTrace::NoDecodeColor = QColor(0x88, 0x8A, 0x85);
const QColor DecodeTrace::ExpandMarkerWarnColor = QColor(0xFF, 0xA5, 0x00); // QColorConstants::Svg::orange
const QColor DecodeTrace::ExpandMarkerHiddenColor = QColor(0x69, 0x69, 0x69); // QColorConstants::Svg::dimgray
const uint8_t DecodeTrace::ExpansionAreaHeaderAlpha = 10 * 255 / 100;
const uint8_t DecodeTrace::ExpansionAreaAlpha = 5 * 255 / 100;

const int DecodeTrace::ArrowSize = 6;
const double DecodeTrace::EndCapWidth = 5;
const int DecodeTrace::RowTitleMargin = 7;
const int DecodeTrace::DrawPadding = 100;

const int DecodeTrace::MaxTraceUpdateRate = 1; // No more than 1 Hz
const int DecodeTrace::AnimationDurationInTicks = 7;
const int DecodeTrace::HiddenRowHideDelay = 1000; // 1 second

/**
 * Helper function for forceUpdate()
 */
void invalidateLayout(QLayout* layout)
{
	// Recompute the given layout and all its child layouts recursively
	for (int i = 0; i < layout->count(); i++) {
		QLayoutItem *item = layout->itemAt(i);

		if (item->layout())
			invalidateLayout(item->layout());
		else
			item->invalidate();
	}

	layout->invalidate();
	layout->activate();
}

void forceUpdate(QWidget* widget)
{
	// Update all child widgets recursively
	for (QObject* child : widget->children())
		if (child->isWidgetType())
			forceUpdate((QWidget*)child);

	// Invalidate the layout of the widget itself
	if (widget->layout())
		invalidateLayout(widget->layout());
}


ContainerWidget::ContainerWidget(QWidget *parent) :
	QWidget(parent)
{
}

void ContainerWidget::resizeEvent(QResizeEvent* event)
{
	QWidget::resizeEvent(event);

	widgetResized(this);
}


DecodeTrace::DecodeTrace(pv::Session &session,
	shared_ptr<data::SignalBase> signalbase, int index) :
	Trace(signalbase),
	session_(session),
	show_hidden_rows_(false),
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
	const int s = DECODE_COLOR_SATURATION;
	const int v = DECODE_COLOR_VALUE;
	color.setHsv(h, s, v);
	base_->set_color(color);

	connect(decode_signal_.get(), SIGNAL(color_changed(QColor)),
		this, SLOT(on_color_changed(QColor)));

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
	connect(&class_show_hide_mapper_, SIGNAL(mapped(QWidget*)),
		this, SLOT(on_show_hide_class(QWidget*)));

	connect(&delayed_trace_updater_, SIGNAL(timeout()),
		this, SLOT(on_delayed_trace_update()));
	delayed_trace_updater_.setSingleShot(true);
	delayed_trace_updater_.setInterval(1000 / MaxTraceUpdateRate);

	connect(&animation_timer_, SIGNAL(timeout()),
		this, SLOT(on_animation_timer()));
	animation_timer_.setInterval(1000 / 50);

	connect(&delayed_hidden_row_hider_, SIGNAL(timeout()),
		this, SLOT(on_hide_hidden_rows()));
	delayed_hidden_row_hider_.setSingleShot(true);
	delayed_hidden_row_hider_.setInterval(HiddenRowHideDelay);

	default_marker_shape_ << QPoint(0,         -ArrowSize);
	default_marker_shape_ << QPoint(ArrowSize,  0);
	default_marker_shape_ << QPoint(0,          ArrowSize);
}

DecodeTrace::~DecodeTrace()
{
	GlobalSettings::remove_change_handler(this);

	for (DecodeTraceRow& r : rows_) {
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

void DecodeTrace::set_owner(TraceTreeItemOwner *owner)
{
	Trace::set_owner(owner);

	// The owner is set in trace::View::signals_changed(), which is a slot.
	// So after this trace was added to the view, we won't have an owner
	// that we need to initialize in update_rows(). Once we do, we call it
	// from on_decode_reset().
	on_decode_reset();
}

pair<int, int> DecodeTrace::v_extents() const
{
	// Make an empty decode trace appear symmetrical
	if (visible_rows_ == 0)
		return make_pair(-default_row_height_, default_row_height_);

	unsigned int height = 0;
	for (const DecodeTraceRow& r : rows_)
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
	unsigned int visible_rows;

#if DECODETRACE_SHOW_RENDER_TIME
	render_time_.restart();
#endif

	// Set default pen to allow for text width calculation
	p.setPen(Qt::black);

	pair<uint64_t, uint64_t> sample_range = get_view_sample_range(pp.left(), pp.right());

	// Just because the view says we see a certain sample range it
	// doesn't mean we have this many decoded samples, too, so crop
	// the range to what has been decoded already
	sample_range.second = min((int64_t)sample_range.second,
		decode_signal_->get_decoded_sample_count(current_segment_, false));

	visible_rows = 0;
	int y = get_visual_y();

	for (DecodeTraceRow& r : rows_) {
		// If the row is hidden, we don't want to fetch annotations
		assert(r.decode_row);
		assert(r.decode_row->decoder());
		if ((!r.decode_row->decoder()->visible()) ||
			((!r.decode_row->visible() && (!show_hidden_rows_) && (!r.expanding) && (!r.expanded) && (!r.collapsing)))) {
			r.currently_visible = false;
			continue;
		}

		deque<const Annotation*> annotations;
		decode_signal_->get_annotation_subset(annotations, r.decode_row,
			current_segment_, sample_range.first, sample_range.second);

		// Show row if there are visible annotations, when user wants to see
		// all rows that have annotations somewhere and this one is one of them
		// or when the row has at least one hidden annotation class
		r.currently_visible = !annotations.empty();
		if (!r.currently_visible) {
			size_t ann_count = decode_signal_->get_annotation_count(r.decode_row, current_segment_);
			r.currently_visible = ((always_show_all_rows_ || r.has_hidden_classes) &&
				(ann_count > 0)) || r.expanded;
		}

		if (r.currently_visible) {
			draw_annotations(annotations, p, pp, y, r);
			y += r.height;
			visible_rows++;
		}
	}

	draw_unresolved_period(p, pp.left(), pp.right());

	if (visible_rows != visible_rows_) {
		visible_rows_ = visible_rows;

		// Call order is important, otherwise the lazy event handler won't work
		owner_->extents_changed(false, true);
		owner_->row_item_appearance_changed(false, true);
	}

	const QString err = decode_signal_->error_message();
	if (!err.isEmpty())
		draw_error(p, err, pp);

#if DECODETRACE_SHOW_RENDER_TIME
	qDebug() << "Rendering" << base_->name() << "took" << render_time_.elapsed() << "ms";
#endif
}

void DecodeTrace::paint_fore(QPainter &p, ViewItemPaintParams &pp)
{
	unsigned int y = get_visual_y();

	update_expanded_rows();

	for (const DecodeTraceRow& r : rows_) {
		if (!r.currently_visible)
			continue;

		p.setPen(QPen(Qt::NoPen));

		if (r.expand_marker_highlighted)
			p.setBrush(QApplication::palette().brush(QPalette::Highlight));
		else if (!r.decode_row->visible())
			p.setBrush(ExpandMarkerHiddenColor);
		else if (r.has_hidden_classes)
			p.setBrush(ExpandMarkerWarnColor);
		else
			p.setBrush(QApplication::palette().brush(QPalette::WindowText));

		// Draw expansion marker
		QPolygon marker(r.expand_marker_shape);
		marker.translate(pp.left(), y);
		p.drawPolygon(marker);

		p.setBrush(QApplication::palette().brush(QPalette::WindowText));

		const QRect text_rect(pp.left() + ArrowSize * 2, y - r.height / 2,
			pp.right() - pp.left(), r.height);
		const QString h(r.decode_row->title());
		const int f = Qt::AlignLeft | Qt::AlignVCenter |
			Qt::TextDontClip;

		// Draw the outline
		p.setPen(QApplication::palette().color(QPalette::Base));
		for (int dx = -1; dx <= 1; dx++)
			for (int dy = -1; dy <= 1; dy++)
				if (dx != 0 && dy != 0)
					p.drawText(text_rect.translated(dx, dy), f, h);

		// Draw the text
		if (!r.decode_row->visible())
			p.setPen(ExpandMarkerHiddenColor);
		else
			p.setPen(QApplication::palette().color(QPalette::WindowText));

		p.drawText(text_rect, f, h);

		y += r.height;
	}

	if (show_hover_marker_)
		paint_hover_marker(p);
}

void DecodeTrace::update_stack_button()
{
	const vector< shared_ptr<Decoder> > &stack = decode_signal_->decoder_stack();

	// Only show decoders in the menu that can be stacked onto the last one in the stack
	if (!stack.empty()) {
		const srd_decoder* d = stack.back()->get_srd_decoder();

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
	const DecodeTraceRow* r = get_row_at_point(click_pos);
	if (r)
		selected_row_ = r->decode_row;

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

	DecodeTraceRow* hover_row = get_row_at_point(hp);

	// Row expansion marker handling
	for (DecodeTraceRow& r : rows_)
		r.expand_marker_highlighted = false;

	if (hover_row) {
		int row_y = get_row_y(hover_row);
		if ((hp.x() > 0) && (hp.x() < (int)(ArrowSize + 3 + hover_row->title_width)) &&
			(hp.y() > (int)(row_y - ArrowSize)) && (hp.y() < (int)(row_y + ArrowSize))) {

			hover_row->expand_marker_highlighted = true;
			show_hidden_rows_ = true;
			delayed_hidden_row_hider_.start();
		}
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
	// Update container widths which depend on the scrollarea's current width
	update_expanded_rows();

	// Handle row expansion marker
	for (DecodeTraceRow& r : rows_) {
		if (!r.expand_marker_highlighted)
			continue;

		unsigned int y = get_row_y(&r);
		if ((event->x() > 0) && (event->x() <= (int)(ArrowSize + 3 + r.title_width)) &&
			(event->y() > (int)(y - (default_row_height_ / 2))) &&
			(event->y() <= (int)(y + (default_row_height_ / 2)))) {

			if (r.expanded) {
				r.collapsing = true;
				r.expanded = false;
				r.anim_shape = ArrowSize;
			} else {
				r.expanding = true;
				r.anim_shape = 0;

				// Force geometry update of the widget container to get
				// an up-to-date height (which also depends on the width)
				forceUpdate(r.container);

				r.container->setVisible(true);
				r.expanded_height = 2 * default_row_height_ + r.container->sizeHint().height();
			}

			r.animation_step = 0;
			r.anim_height = r.height;

			animation_timer_.start();
		}
	}
}

void DecodeTrace::draw_annotations(deque<const Annotation*>& annotations,
		QPainter &p, const ViewItemPaintParams &pp, int y, const DecodeTraceRow& row)
{
	Annotation::Class block_class = 0;
	bool block_class_uniform = true;
	qreal block_start = 0;
	int block_ann_count = 0;

	const Annotation* prev_ann;
	qreal prev_end = INT_MIN;

	qreal a_end;

	double samples_per_pixel, pixels_offset;
	tie(pixels_offset, samples_per_pixel) =
		get_pixels_offset_samples_per_pixel();

	// Gather all annotations that form a visual "block" and draw them as such
	for (const Annotation* a : annotations) {

		const qreal abs_a_start = a->start_sample() / samples_per_pixel;
		const qreal abs_a_end   = a->end_sample() / samples_per_pixel;

		const qreal a_start = abs_a_start - pixels_offset;
		a_end = abs_a_end - pixels_offset;

		const qreal a_width = a_end - a_start;
		const qreal delta = a_end - prev_end;

		bool a_is_separate = false;

		// Annotation wider than the threshold for a useful label width?
		if (a_width >= min_useful_label_width_) {
			for (const QString &ann_text : *(a->annotations())) {
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
				draw_annotation(prev_ann, p, pp, y, row);
			else if (block_ann_count > 0)
				draw_annotation_block(block_start, prev_end, block_class,
					block_class_uniform, p, y, row);

			block_ann_count = 0;
		}

		if (a_is_separate) {
			draw_annotation(a, p, pp, y, row);
			// Next annotation must start a new block. delta will be > 1
			// because we set prev_end to INT_MIN but that's okay since
			// block_ann_count will be 0 and nothing will be drawn
			prev_end = INT_MIN;
			block_ann_count = 0;
		} else {
			prev_end = a_end;
			prev_ann = a;

			if (block_ann_count == 0) {
				block_start = a_start;
				block_class = a->ann_class_id();
				block_class_uniform = true;
			} else
				if (a->ann_class_id() != block_class)
					block_class_uniform = false;

			block_ann_count++;
		}
	}

	if (block_ann_count == 1)
		draw_annotation(prev_ann, p, pp, y, row);
	else if (block_ann_count > 0)
		draw_annotation_block(block_start, prev_end, block_class,
			block_class_uniform, p, y, row);
}

void DecodeTrace::draw_annotation(const Annotation* a, QPainter &p,
	const ViewItemPaintParams &pp, int y, const DecodeTraceRow& row) const
{
	double samples_per_pixel, pixels_offset;
	tie(pixels_offset, samples_per_pixel) =
		get_pixels_offset_samples_per_pixel();

	const double start = a->start_sample() / samples_per_pixel - pixels_offset;
	const double end = a->end_sample() / samples_per_pixel - pixels_offset;

	p.setPen(a->dark_color());
	p.setBrush(a->color());

	if ((start > (pp.right() + DrawPadding)) || (end < (pp.left() - DrawPadding)))
		return;

	if (a->start_sample() == a->end_sample())
		draw_instant(a, p, start, y);
	else
		draw_range(a, p, start, end, y, pp, row.title_width);
}

void DecodeTrace::draw_annotation_block(qreal start, qreal end,
	Annotation::Class ann_class, bool use_ann_format, QPainter &p, int y,
	const DecodeTraceRow& row) const
{
	const double top = y + .5 - annotation_height_ / 2;
	const double bottom = y + .5 + annotation_height_ / 2;
	const double width = end - start;

	// If all annotations in this block are of the same type, we can use the
	// one format that all of these annotations have. Otherwise, we should use
	// a neutral color (i.e. gray)
	if (use_ann_format) {
		p.setPen(row.decode_row->get_dark_class_color(ann_class));
		p.setBrush(QBrush(row.decode_row->get_class_color(ann_class), Qt::Dense4Pattern));
	} else {
		p.setPen(QColor(Qt::darkGray));
		p.setBrush(QBrush(Qt::gray, Qt::Dense4Pattern));
	}

	if (width <= 1)
		p.drawLine(QPointF(start, top), QPointF(start, bottom));
	else {
		const QRectF rect(start, top, width, bottom - top);
		const int r = annotation_height_ / 4;
		p.drawRoundedRect(rect, r, r);
	}
}

void DecodeTrace::draw_instant(const Annotation* a, QPainter &p, qreal x, int y) const
{
	const QString text = a->annotations()->empty() ?
		QString() : a->annotations()->back();
	const qreal w = min((qreal)p.boundingRect(QRectF(), 0, text).width(),
		0.0) + annotation_height_;
	const QRectF rect(x - w / 2, y - annotation_height_ / 2, w, annotation_height_);

	p.drawRoundedRect(rect, annotation_height_ / 2, annotation_height_ / 2);

	p.setPen(Qt::black);
	p.drawText(rect, Qt::AlignCenter | Qt::AlignVCenter, text);
}

void DecodeTrace::draw_range(const Annotation* a, QPainter &p,
	qreal start, qreal end, int y, const ViewItemPaintParams &pp,
	int row_title_width) const
{
	const qreal top = y + .5 - annotation_height_ / 2;
	const qreal bottom = y + .5 + annotation_height_ / 2;
	const vector<QString>* annotations = a->annotations();

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

	if (annotations->empty())
		return;

	const int ann_start = start + cap_width;
	const int ann_end = end - cap_width;

	const int real_start = max(ann_start, pp.left() + ArrowSize + row_title_width);
	const int real_end = min(ann_end, pp.right());
	const int real_width = real_end - real_start;

	QRectF rect(real_start, y - annotation_height_ / 2, real_width, annotation_height_);
	if (rect.width() <= 4)
		return;

	p.setPen(Qt::black);

	// Try to find an annotation that will fit
	QString best_annotation;
	int best_width = 0;

	for (const QString &s : *annotations) {
		const int w = p.boundingRect(QRectF(), 0, s).width();
		if (w <= rect.width() && w > best_width)
			best_annotation = s, best_width = w;
	}

	if (best_annotation.isEmpty())
		best_annotation = annotations->back();

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

void DecodeTrace::draw_unresolved_period(QPainter &p, int left, int right) const
{
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
	const QRectF no_decode_rect(start, y - (annotation_height_ / 2) - 0.5,
		end - start, annotation_height_);

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

	double samplerate = decode_signal_->get_samplerate();

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

unsigned int DecodeTrace::get_row_y(const DecodeTraceRow* row) const
{
	assert(row);

	unsigned int y = get_visual_y();

	for (const DecodeTraceRow& r : rows_) {
		if (!r.currently_visible)
			continue;

		if (row->decode_row == r.decode_row)
			break;
		else
			y += r.height;
	}

	return y;
}

DecodeTraceRow* DecodeTrace::get_row_at_point(const QPoint &point)
{
	int y = get_visual_y() - (default_row_height_ / 2);

	for (DecodeTraceRow& r : rows_) {
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
	if (!enabled())
		return QString();

	const pair<uint64_t, uint64_t> sample_range =
		get_view_sample_range(point.x(), point.x() + 1);
	const DecodeTraceRow* r = get_row_at_point(point);

	if (!r)
		return QString();

	if (point.y() > (int)(get_row_y(r) + (annotation_height_ / 2)))
		return QString();

	deque<const Annotation*> annotations;

	decode_signal_->get_annotation_subset(annotations, r->decode_row,
		current_segment_, sample_range.first, sample_range.second);

	return (annotations.empty()) ?
		QString() : annotations[0]->annotations()->front();
}

void DecodeTrace::create_decoder_form(int index, shared_ptr<Decoder> &dec,
	QWidget *parent, QFormLayout *form)
{
	GlobalSettings settings;

	assert(dec);
	const srd_decoder *const decoder = dec->get_srd_decoder();
	assert(decoder);

	const bool decoder_deletable = index > 0;

	pv::widgets::DecoderGroupBox *const group =
		new pv::widgets::DecoderGroupBox(
			QString::fromUtf8(decoder->name),
			tr("%1:\n%2").arg(QString::fromUtf8(decoder->longname),
				QString::fromUtf8(decoder->desc)),
			nullptr, decoder_deletable);
	group->set_decoder_visible(dec->visible());

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

void DecodeTrace::export_annotations(deque<const Annotation*>& annotations) const
{
	GlobalSettings settings;
	const QString dir = settings.value("MainWindow/SaveDirectory").toString();

	const QString file_name = QFileDialog::getSaveFileName(
		owner_->view(), tr("Export annotations"), dir, tr("Text Files (*.txt);;All Files (*)"));

	if (file_name.isEmpty())
		return;

	QString format = settings.value(GlobalSettings::Key_Dec_ExportFormat).toString();
	const QString quote = format.contains("%q") ? "\"" : "";
	format = format.remove("%q");

	const bool has_sample_range   = format.contains("%s");
	const bool has_row_name       = format.contains("%r");
	const bool has_dec_name       = format.contains("%d");
	const bool has_class_name     = format.contains("%c");
	const bool has_first_ann_text = format.contains("%1");
	const bool has_all_ann_text   = format.contains("%a");

	QFile file(file_name);
	if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
		QTextStream out_stream(&file);

		for (const Annotation* ann : annotations) {
			QString out_text = format;

			if (has_sample_range) {
				const QString sample_range = QString("%1-%2") \
					.arg(QString::number(ann->start_sample()), QString::number(ann->end_sample()));
				out_text = out_text.replace("%s", sample_range);
			}

			if (has_dec_name)
				out_text = out_text.replace("%d",
					quote + QString::fromUtf8(ann->row()->decoder()->name()) + quote);

			if (has_row_name) {
				const QString row_name = quote + ann->row()->description() + quote;
				out_text = out_text.replace("%r", row_name);
			}

			if (has_class_name) {
				const QString class_name = quote + ann->ann_class_name() + quote;
				out_text = out_text.replace("%c", class_name);
			}

			if (has_first_ann_text) {
				const QString first_ann_text = quote + ann->annotations()->front() + quote;
				out_text = out_text.replace("%1", first_ann_text);
			}

			if (has_all_ann_text) {
				QString all_ann_text;
				for (const QString &s : *(ann->annotations()))
					all_ann_text = all_ann_text + quote + s + quote + ",";
				all_ann_text.chop(1);

				out_text = out_text.replace("%a", all_ann_text);
			}

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

void DecodeTrace::initialize_row_widgets(DecodeTraceRow* r, unsigned int row_id)
{
	// Set colors and fixed widths
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

	const int w = m.boundingRect(r->decode_row->title()).width() + RowTitleMargin;
	r->title_width = w;

	// Set up top-level container
	connect(r->container, SIGNAL(widgetResized(QWidget*)),
		this, SLOT(on_row_container_resized(QWidget*)));

	QVBoxLayout* vlayout = new QVBoxLayout();
	r->container->setLayout(vlayout);

	// Add header container
	vlayout->addWidget(r->header_container);
	vlayout->setContentsMargins(0, 0, 0, 0);
	vlayout->setSpacing(0);
	QHBoxLayout* header_container_layout = new QHBoxLayout();
	r->header_container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	r->header_container->setMinimumSize(0, default_row_height_);
	r->header_container->setLayout(header_container_layout);
	r->header_container->layout()->setContentsMargins(10, 2, 10, 2);

	r->header_container->setAutoFillBackground(true);
	r->header_container->setPalette(header_palette);

	// Add widgets inside the header container
	QCheckBox* cb = new QCheckBox();
	r->row_visibility_checkbox = cb;
	header_container_layout->addWidget(cb);
	cb->setText(tr("Show this row"));
	cb->setChecked(r->decode_row->visible());

	row_show_hide_mapper_.setMapping(cb, row_id);
	connect(cb, SIGNAL(stateChanged(int)),
		&row_show_hide_mapper_, SLOT(map()));

	QPushButton* btn = new QPushButton();
	header_container_layout->addWidget(btn);
	btn->setFlat(true);
	btn->setStyleSheet(":hover { background-color: palette(button); color: palette(button-text); border:0; }");
	btn->setText(tr("Show All"));
	btn->setProperty("decode_trace_row_ptr", QVariant::fromValue((void*)r));
	connect(btn, SIGNAL(clicked(bool)), this, SLOT(on_show_all_classes()));

	btn = new QPushButton();
	header_container_layout->addWidget(btn);
	btn->setFlat(true);
	btn->setStyleSheet(":hover { background-color: palette(button); color: palette(button-text); border:0; }");
	btn->setText(tr("Hide All"));
	btn->setProperty("decode_trace_row_ptr", QVariant::fromValue((void*)r));
	connect(btn, SIGNAL(clicked(bool)), this, SLOT(on_hide_all_classes()));

	header_container_layout->addStretch(); // To left-align the header widgets

	// Add selector container
	vlayout->addWidget(r->selector_container);
	r->selector_container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	r->selector_container->setLayout(new FlowLayout(r->selector_container));

	r->selector_container->setAutoFillBackground(true);
	r->selector_container->setPalette(selector_palette);

	// Add all classes that can be toggled
	vector<AnnotationClass*> ann_classes = r->decode_row->ann_classes();

	for (const AnnotationClass* ann_class : ann_classes) {
		cb = new QCheckBox();
		cb->setText(tr(ann_class->description));
		cb->setChecked(ann_class->visible);

		int dim = ViewItemPaintParams::text_height() - 2;
		QPixmap pixmap(dim, dim);
		pixmap.fill(r->decode_row->get_class_color(ann_class->id));
		cb->setIcon(pixmap);

		r->selector_container->layout()->addWidget(cb);
		r->selectors.push_back(cb);

		cb->setProperty("ann_class_ptr", QVariant::fromValue((void*)ann_class));
		cb->setProperty("decode_trace_row_ptr", QVariant::fromValue((void*)r));

		class_show_hide_mapper_.setMapping(cb, cb);
		connect(cb, SIGNAL(stateChanged(int)),
			&class_show_hide_mapper_, SLOT(map()));
	}
}

void DecodeTrace::update_rows()
{
	if (!owner_)
		return;

	lock_guard<mutex> lock(row_modification_mutex_);

	for (DecodeTraceRow& r : rows_)
		r.exists = false;

	unsigned int row_id = 0;
	for (Row* decode_row : decode_signal_->get_rows()) {
		// Find row in our list
		auto r_it = find_if(rows_.begin(), rows_.end(),
			[&](DecodeTraceRow& r){ return r.decode_row == decode_row; });

		DecodeTraceRow* r = nullptr;
		if (r_it == rows_.end()) {
			// Row doesn't exist yet, create and append it
			DecodeTraceRow nr;
			nr.decode_row = decode_row;
			nr.decode_row->set_base_color(base_->color());
			nr.height = default_row_height_;
			nr.expanded_height = default_row_height_;
			nr.currently_visible = false;
			nr.has_hidden_classes = decode_row->has_hidden_classes();
			nr.expand_marker_highlighted = false;
			nr.expanding = false;
			nr.expanded = false;
			nr.collapsing = false;
			nr.expand_marker_shape = default_marker_shape_;
			nr.container = new ContainerWidget(owner_->view()->scrollarea());
			nr.header_container = new QWidget(nr.container);
			nr.selector_container = new QWidget(nr.container);

			rows_.push_back(nr);
			r = &rows_.back();
			initialize_row_widgets(r, row_id);
		} else
			r = &(*r_it);

		r->exists = true;
		row_id++;
	}

	// If there's only one row, it must not be hidden or else it can't be un-hidden
	if (row_id == 1)
		rows_.front().row_visibility_checkbox->setEnabled(false);

	// Remove any rows that no longer exist, obeying that iterators are invalidated
	bool any_exists;
	do {
		any_exists = false;

		for (unsigned int i = 0; i < rows_.size(); i++)
			if (!rows_[i].exists) {
				delete rows_[i].row_visibility_checkbox;

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

void DecodeTrace::set_row_expanded(DecodeTraceRow* r)
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

void DecodeTrace::set_row_collapsed(DecodeTraceRow* r)
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
	for (DecodeTraceRow& r : rows_) {
		if (r.expanding || r.expanded)
			r.expanded_height = 2 * default_row_height_ + r.container->sizeHint().height();

		if (r.expanded)
			r.height = r.expanded_height;

		int x = 2 * ArrowSize;
		int y = get_row_y(&r) + default_row_height_;
		// Only update the position if it actually changes
		if ((x != r.container->pos().x()) || (y != r.container->pos().y()))
			r.container->move(x, y);

		int w = owner_->view()->viewport()->width() - x;
		int h = r.height - 2 * default_row_height_;
		// Only update the dimension if they actually change
		if ((w != r.container->sizeHint().width()) || (h != r.container->sizeHint().height()))
			r.container->resize(w, h);
	}
}

void DecodeTrace::on_setting_changed(const QString &key, const QVariant &value)
{
	Trace::on_setting_changed(key, value);

	if (key == GlobalSettings::Key_Dec_AlwaysShowAllRows)
		always_show_all_rows_ = value.toBool();
}

void DecodeTrace::on_color_changed(const QColor &color)
{
	for (DecodeTraceRow& r : rows_)
		r.decode_row->set_base_color(color);

	if (owner_)
		owner_->row_item_appearance_changed(false, true);
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

	owner_->extents_changed(false, true);

	create_popup_form();
}

void DecodeTrace::on_show_hide_decoder(int index)
{
	const bool state = decode_signal_->toggle_decoder_visibility(index);

	assert(index < (int)decoder_forms_.size());
	decoder_forms_[index]->set_decoder_visible(state);

	if (!state)
		owner_->extents_changed(false, true);

	owner_->row_item_appearance_changed(false, true);
}

void DecodeTrace::on_show_hide_row(int row_id)
{
	if (row_id >= (int)rows_.size())
		return;

	rows_[row_id].decode_row->set_visible(!rows_[row_id].decode_row->visible());

	if (!rows_[row_id].decode_row->visible())
		set_row_collapsed(&rows_[row_id]);

	owner_->extents_changed(false, true);
	owner_->row_item_appearance_changed(false, true);
}

void DecodeTrace::on_show_hide_class(QWidget* sender)
{
	void* ann_class_ptr = sender->property("ann_class_ptr").value<void*>();
	assert(ann_class_ptr);
	AnnotationClass* ann_class = (AnnotationClass*)ann_class_ptr;

	ann_class->visible = !ann_class->visible;

	void* row_ptr = sender->property("decode_trace_row_ptr").value<void*>();
	assert(row_ptr);
	DecodeTraceRow* row = (DecodeTraceRow*)row_ptr;

	row->has_hidden_classes = row->decode_row->has_hidden_classes();

	owner_->row_item_appearance_changed(false, true);
}

void DecodeTrace::on_show_all_classes()
{
	void* row_ptr = QObject::sender()->property("decode_trace_row_ptr").value<void*>();
	assert(row_ptr);
	DecodeTraceRow* row = (DecodeTraceRow*)row_ptr;

	for (QCheckBox* cb : row->selectors)
		cb->setChecked(true);

	row->has_hidden_classes = false;

	owner_->row_item_appearance_changed(false, true);
}

void DecodeTrace::on_hide_all_classes()
{
	void* row_ptr = QObject::sender()->property("decode_trace_row_ptr").value<void*>();
	assert(row_ptr);
	DecodeTraceRow* row = (DecodeTraceRow*)row_ptr;

	for (QCheckBox* cb : row->selectors)
		cb->setChecked(false);

	row->has_hidden_classes = true;

	owner_->row_item_appearance_changed(false, true);
}

void DecodeTrace::on_row_container_resized(QWidget* sender)
{
	sender->update();

	owner_->extents_changed(false, true);
	owner_->row_item_appearance_changed(false, true);
}

void DecodeTrace::on_copy_annotation_to_clipboard()
{
	if (!selected_row_)
		return;

	deque<const Annotation*> annotations;

	decode_signal_->get_annotation_subset(annotations, selected_row_,
		current_segment_, selected_sample_range_.first, selected_sample_range_.first);

	if (annotations.empty())
		return;

	QClipboard *clipboard = QApplication::clipboard();
	clipboard->setText(annotations.front()->annotations()->front(), QClipboard::Clipboard);

	if (clipboard->supportsSelection())
		clipboard->setText(annotations.front()->annotations()->front(), QClipboard::Selection);
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
	if (!selected_row_)
		return;

	deque<const Annotation*> annotations;

	decode_signal_->get_annotation_subset(annotations, selected_row_,
		current_segment_, selected_sample_range_.first, selected_sample_range_.second);

	if (annotations.empty())
		return;

	export_annotations(annotations);
}

void DecodeTrace::on_export_all_rows_from_here()
{
	deque<const Annotation*> annotations;

	decode_signal_->get_annotation_subset(annotations, current_segment_,
			selected_sample_range_.first, selected_sample_range_.second);

	if (!annotations.empty())
		export_annotations(annotations);
}

void DecodeTrace::on_animation_timer()
{
	bool animation_finished = true;

	for (DecodeTraceRow& r : rows_) {
		if (!(r.expanding || r.collapsing))
			continue;

		unsigned int height_delta = r.expanded_height - default_row_height_;

		if (r.expanding) {
			if (r.height < r.expanded_height) {
				r.anim_height += height_delta / (float)AnimationDurationInTicks;
				r.height = min((int)r.anim_height, (int)r.expanded_height);
				r.anim_shape += ArrowSize / (float)AnimationDurationInTicks;
				animation_finished = false;
			} else
				set_row_expanded(&r);
		}

		if (r.collapsing) {
			if (r.height > default_row_height_) {
				r.anim_height -= height_delta / (float)AnimationDurationInTicks;
				r.height = max((int)r.anim_height, (int)0);
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

void DecodeTrace::on_hide_hidden_rows()
{
	// Make all hidden traces invisible again unless the user is hovering over a row name
	bool any_highlighted = false;

	for (DecodeTraceRow& r : rows_)
		if (r.expand_marker_highlighted)
			any_highlighted = true;

	if (!any_highlighted) {
		show_hidden_rows_ = false;

		owner_->extents_changed(false, true);
		owner_->row_item_appearance_changed(false, true);
	}
}

} // namespace trace
} // namespace views
} // namespace pv
