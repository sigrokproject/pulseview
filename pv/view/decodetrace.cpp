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

extern "C" {
#include <libsigrokdecode/libsigrokdecode.h>
}

#include <extdef.h>

#include <tuple>

#include <boost/functional/hash.hpp>

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QToolTip>

#include "decodetrace.h"

#include <pv/sigsession.h>
#include <pv/data/decoderstack.h>
#include <pv/data/decode/decoder.h>
#include <pv/data/logic.h>
#include <pv/data/logicsnapshot.h>
#include <pv/data/decode/annotation.h>
#include <pv/view/logicsignal.h>
#include <pv/view/view.h>
#include <pv/view/viewport.h>
#include <pv/widgets/decodergroupbox.h>
#include <pv/widgets/decodermenu.h>

using std::dynamic_pointer_cast;
using std::list;
using std::make_pair;
using std::max;
using std::map;
using std::min;
using std::pair;
using std::shared_ptr;
using std::tie;
using std::vector;

namespace pv {
namespace view {

const QColor DecodeTrace::DecodeColours[4] = {
	QColor(0xEF, 0x29, 0x29),	// Red
	QColor(0xFC, 0xE9, 0x4F),	// Yellow
	QColor(0x8A, 0xE2, 0x34),	// Green
	QColor(0x72, 0x9F, 0xCF)	// Blue
};

const QColor DecodeTrace::ErrorBgColour = QColor(0xEF, 0x29, 0x29);
const QColor DecodeTrace::NoDecodeColour = QColor(0x88, 0x8A, 0x85);

const int DecodeTrace::ArrowSize = 4;
const double DecodeTrace::EndCapWidth = 5;
const int DecodeTrace::DrawPadding = 100;

const QColor DecodeTrace::Colours[16] = {
	QColor(0xEF, 0x29, 0x29),
	QColor(0xF6, 0x6A, 0x32),
	QColor(0xFC, 0xAE, 0x3E),
	QColor(0xFB, 0xCA, 0x47),
	QColor(0xFC, 0xE9, 0x4F),
	QColor(0xCD, 0xF0, 0x40),
	QColor(0x8A, 0xE2, 0x34),
	QColor(0x4E, 0xDC, 0x44),
	QColor(0x55, 0xD7, 0x95),
	QColor(0x64, 0xD1, 0xD2),
	QColor(0x72, 0x9F, 0xCF),
	QColor(0xD4, 0x76, 0xC4),
	QColor(0x9D, 0x79, 0xB9),
	QColor(0xAD, 0x7F, 0xA8),
	QColor(0xC2, 0x62, 0x9B),
	QColor(0xD7, 0x47, 0x6F)
};

const QColor DecodeTrace::OutlineColours[16] = {
	QColor(0x77, 0x14, 0x14),
	QColor(0x7B, 0x35, 0x19),
	QColor(0x7E, 0x57, 0x1F),
	QColor(0x7D, 0x65, 0x23),
	QColor(0x7E, 0x74, 0x27),
	QColor(0x66, 0x78, 0x20),
	QColor(0x45, 0x71, 0x1A),
	QColor(0x27, 0x6E, 0x22),
	QColor(0x2A, 0x6B, 0x4A),
	QColor(0x32, 0x68, 0x69),
	QColor(0x39, 0x4F, 0x67),
	QColor(0x6A, 0x3B, 0x62),
	QColor(0x4E, 0x3C, 0x5C),
	QColor(0x56, 0x3F, 0x54),
	QColor(0x61, 0x31, 0x4D),
	QColor(0x6B, 0x23, 0x37)
};

DecodeTrace::DecodeTrace(pv::SigSession &session,
	std::shared_ptr<pv::data::DecoderStack> decoder_stack, int index) :
	Trace(QString::fromUtf8(
		decoder_stack->stack().front()->decoder()->name)),
	_session(session),
	_decoder_stack(decoder_stack),
	_text_height(0),
	_row_height(0),
	_delete_mapper(this),
	_show_hide_mapper(this)
{
	assert(_decoder_stack);

	_colour = DecodeColours[index % countof(DecodeColours)];

	connect(_decoder_stack.get(), SIGNAL(new_decode_data()),
		this, SLOT(on_new_decode_data()));
	connect(&_delete_mapper, SIGNAL(mapped(int)),
		this, SLOT(on_delete_decoder(int)));
	connect(&_show_hide_mapper, SIGNAL(mapped(int)),
		this, SLOT(on_show_hide_decoder(int)));
}

bool DecodeTrace::enabled() const
{
	return true;
}

const std::shared_ptr<pv::data::DecoderStack>& DecodeTrace::decoder() const
{
	return _decoder_stack;
}

void DecodeTrace::paint_back(QPainter &p, int left, int right)
{
	Trace::paint_back(p, left, right);
	paint_axis(p, get_y(), left, right);
}

void DecodeTrace::paint_mid(QPainter &p, int left, int right)
{
	using namespace pv::data::decode;

	QFontMetrics m(QApplication::font());
	_text_height = m.boundingRect(QRect(), 0, "Tg").height();
	_row_height = (_text_height * 6) / 4;
	const int annotation_height = (_text_height * 5) / 4;

	assert(_decoder_stack);
	const QString err = _decoder_stack->error_message();
	if (!err.isEmpty())
	{
		draw_unresolved_period(p, annotation_height, left, right);
		draw_error(p, err, left, right);
		return;
	}

	// Iterate through the rows
	int y = get_y();
	pair<uint64_t, uint64_t> sample_range = get_sample_range(left, right);

	assert(_decoder_stack);
	const vector<Row> rows(_decoder_stack->get_visible_rows());

	_visible_rows.clear();
	for (size_t i = 0; i < rows.size(); i++)
	{
		const Row &row = rows[i];

		size_t base_colour = 0x13579BDF;
		boost::hash_combine(base_colour, this);
		boost::hash_combine(base_colour, row.decoder());
		boost::hash_combine(base_colour, row.row());
		base_colour >>= 16;

		vector<Annotation> annotations;
		_decoder_stack->get_annotation_subset(annotations, row,
			sample_range.first, sample_range.second);
		if (!annotations.empty()) {
			for (const Annotation &a : annotations)
				draw_annotation(a, p, get_text_colour(),
					annotation_height, left, right, y,
					base_colour);
			y += _row_height;

			_visible_rows.push_back(rows[i]);
		}
	}

	// Draw the hatching
	draw_unresolved_period(p, annotation_height, left, right);
}

void DecodeTrace::paint_fore(QPainter &p, int left, int right)
{
	using namespace pv::data::decode;

	(void)right;

	assert(_row_height);

	for (size_t i = 0; i < _visible_rows.size(); i++)
	{
		const int y = i * _row_height + get_y();

		p.setPen(QPen(Qt::NoPen));
		p.setBrush(QApplication::palette().brush(QPalette::WindowText));

		if (i != 0)
		{
			const QPointF points[] = {
				QPointF(left, y - ArrowSize),
				QPointF(left + ArrowSize, y),
				QPointF(left, y + ArrowSize)
			};
			p.drawPolygon(points, countof(points));
		}

		const QRect r(left + ArrowSize * 2, y - _row_height / 2,
			right - left, _row_height);
		const QString h(_visible_rows[i].title());
		const int f = Qt::AlignLeft | Qt::AlignVCenter |
			Qt::TextDontClip;

		// Draw the outline
		p.setPen(QApplication::palette().color(QPalette::Base));
		for (int dx = -1; dx <= 1; dx++)
			for (int dy = -1; dy <= 1; dy++)
				if (dx != 0 && dy != 0)
					p.drawText(r.translated(dx, dy), f, h);

		// Draw the text
		p.setPen(QApplication::palette().color(QPalette::WindowText));
		p.drawText(r, f, h);
	}
}

void DecodeTrace::populate_popup_form(QWidget *parent, QFormLayout *form)
{
	using pv::data::decode::Decoder;

	assert(form);
	assert(parent);
	assert(_decoder_stack);

	// Add the standard options
	Trace::populate_popup_form(parent, form);

	// Add the decoder options
	_bindings.clear();
	_channel_selectors.clear();
	_decoder_forms.clear();

	const list< shared_ptr<Decoder> >& stack = _decoder_stack->stack();

	if (stack.empty())
	{
		QLabel *const l = new QLabel(
			tr("<p><i>No decoders in the stack</i></p>"));
		l->setAlignment(Qt::AlignCenter);
		form->addRow(l);
	}
	else
	{
		auto iter = stack.cbegin();
		for (int i = 0; i < (int)stack.size(); i++, iter++) {
			shared_ptr<Decoder> dec(*iter);
			create_decoder_form(i, dec, parent, form);
		}

		form->addRow(new QLabel(
			tr("<i>* Required channels</i>"), parent));
	}

	// Add stacking button
	pv::widgets::DecoderMenu *const decoder_menu =
		new pv::widgets::DecoderMenu(parent);
	connect(decoder_menu, SIGNAL(decoder_selected(srd_decoder*)),
		this, SLOT(on_stack_decoder(srd_decoder*)));

	QPushButton *const stack_button =
		new QPushButton(tr("Stack Decoder"), parent);
	stack_button->setMenu(decoder_menu);

	QHBoxLayout *stack_button_box = new QHBoxLayout;
	stack_button_box->addWidget(stack_button, 0, Qt::AlignRight);
	form->addRow(stack_button_box);
}

QMenu* DecodeTrace::create_context_menu(QWidget *parent)
{
	QMenu *const menu = Trace::create_context_menu(parent);

	menu->addSeparator();

	QAction *const del = new QAction(tr("Delete"), this);
	del->setShortcuts(QKeySequence::Delete);
	connect(del, SIGNAL(triggered()), this, SLOT(on_delete()));
	menu->addAction(del);

	return menu;
}

void DecodeTrace::draw_annotation(const pv::data::decode::Annotation &a,
	QPainter &p, QColor text_color, int h, int left, int right, int y,
	size_t base_colour) const
{
	double samples_per_pixel, pixels_offset;
	tie(pixels_offset, samples_per_pixel) =
		get_pixels_offset_samples_per_pixel();

	const double start = a.start_sample() / samples_per_pixel -
		pixels_offset;
	const double end = a.end_sample() / samples_per_pixel -
		pixels_offset;

	const size_t colour = (base_colour + a.format()) % countof(Colours);
	const QColor &fill = Colours[colour];
	const QColor &outline = OutlineColours[colour];

	if (start > right + DrawPadding || end < left - DrawPadding)
		return;

	if (a.start_sample() == a.end_sample())
		draw_instant(a, p, fill, outline, text_color, h,
			start, y);
	else
		draw_range(a, p, fill, outline, text_color, h,
			start, end, y);
}

void DecodeTrace::draw_instant(const pv::data::decode::Annotation &a, QPainter &p,
	QColor fill, QColor outline, QColor text_color, int h, double x, int y) const
{
	const QString text = a.annotations().empty() ?
		QString() : a.annotations().back();
	const double w = min((double)p.boundingRect(QRectF(), 0, text).width(),
		0.0) + h;
	const QRectF rect(x - w / 2, y - h / 2, w, h);

	p.setPen(outline);
	p.setBrush(fill);
	p.drawRoundedRect(rect, h / 2, h / 2);

	p.setPen(text_color);
	p.drawText(rect, Qt::AlignCenter | Qt::AlignVCenter, text);
}

void DecodeTrace::draw_range(const pv::data::decode::Annotation &a, QPainter &p,
	QColor fill, QColor outline, QColor text_color, int h, double start,
	double end, int y) const
{
	const double top = y + .5 - h / 2;
	const double bottom = y + .5 + h / 2;
	const vector<QString> annotations = a.annotations();

	p.setPen(outline);
	p.setBrush(fill);

	// If the two ends are within 1 pixel, draw a vertical line
	if (start + 1.0 > end)
	{
		p.drawLine(QPointF(start, top), QPointF(start, bottom));
		return;
	}

	const double cap_width = min((end - start) / 4, EndCapWidth);

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

	QRectF rect(start + cap_width, y - h / 2,
		end - start - cap_width * 2, h);
	if (rect.width() <= 4)
		return;

	p.setPen(text_color);

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
	int left, int right)
{
	const int y = get_y();

	p.setPen(ErrorBgColour.darker());
	p.setBrush(ErrorBgColour);

	const QRectF bounding_rect =
		QRectF(left, INT_MIN / 2 + y, right - left, INT_MAX);
	const QRectF text_rect = p.boundingRect(bounding_rect,
		Qt::AlignCenter, message);
	const float r = text_rect.height() / 4;

	p.drawRoundedRect(text_rect.adjusted(-r, -r, r, r), r, r,
		Qt::AbsoluteSize);

	p.setPen(get_text_colour());
	p.drawText(text_rect, message);
}

void DecodeTrace::draw_unresolved_period(QPainter &p, int h, int left,
	int right) const
{
	using namespace pv::data;
	using pv::data::decode::Decoder;

	double samples_per_pixel, pixels_offset;

	assert(_decoder_stack);	

	shared_ptr<Logic> data;
	shared_ptr<LogicSignal> logic_signal;

	const list< shared_ptr<Decoder> > &stack = _decoder_stack->stack();

	// We get the logic data of the first channel in the list.
	// This works because we are currently assuming all
	// LogicSignals have the same data/snapshot
	for (const shared_ptr<Decoder> &dec : stack)
		if (dec && !dec->channels().empty() &&
			((logic_signal = (*dec->channels().begin()).second)) &&
			((data = logic_signal->logic_data())))
			break;

	if (!data || data->get_snapshots().empty())
		return;

	const shared_ptr<LogicSnapshot> snapshot =
		data->get_snapshots().front();
	assert(snapshot);
	const int64_t sample_count = (int64_t)snapshot->get_sample_count();
	if (sample_count == 0)
		return;

	const int64_t samples_decoded = _decoder_stack->samples_decoded();
	if (sample_count == samples_decoded)
		return;

	const int y = get_y();

	tie(pixels_offset, samples_per_pixel) =
		get_pixels_offset_samples_per_pixel();

	const double start = max(samples_decoded /
		samples_per_pixel - pixels_offset, left - 1.0);
	const double end = min(sample_count / samples_per_pixel -
		pixels_offset, right + 1.0);
	const QRectF no_decode_rect(start, y - h/2 + 0.5, end - start, h);

	p.setPen(QPen(Qt::NoPen));
	p.setBrush(Qt::white);
	p.drawRect(no_decode_rect);

	p.setPen(NoDecodeColour);
	p.setBrush(QBrush(NoDecodeColour, Qt::Dense6Pattern));
	p.drawRect(no_decode_rect);
}

pair<double, double> DecodeTrace::get_pixels_offset_samples_per_pixel() const
{
	assert(_view);
	assert(_decoder_stack);

	const double scale = _view->scale();
	assert(scale > 0);

	const double pixels_offset =
		(_view->offset() - _decoder_stack->get_start_time()) / scale;

	double samplerate = _decoder_stack->samplerate();

	// Show sample rate as 1Hz when it is unknown
	if (samplerate == 0.0)
		samplerate = 1.0;

	return make_pair(pixels_offset, samplerate * scale);
}

pair<uint64_t, uint64_t> DecodeTrace::get_sample_range(
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

int DecodeTrace::get_row_at_point(const QPoint &point)
{
	if (!_row_height)
		return -1;

	const int row = (point.y() - get_y() + _row_height / 2) / _row_height;
	if (row < 0 || row >= (int)_visible_rows.size())
		return -1;

	return row;
}

const QString DecodeTrace::get_annotation_at_point(const QPoint &point)
{
	using namespace pv::data::decode;

	if (!enabled())
		return QString();

	const pair<uint64_t, uint64_t> sample_range =
		get_sample_range(point.x(), point.x() + 1);
	const int row = get_row_at_point(point);
	if (row < 0)
		return QString();

	vector<pv::data::decode::Annotation> annotations;

	assert(_decoder_stack);
	_decoder_stack->get_annotation_subset(annotations, _visible_rows[row],
		sample_range.first, sample_range.second);

	return (annotations.empty()) ?
		QString() : annotations[0].annotations().front();
}

void DecodeTrace::hide_hover_annotation()
{
	QToolTip::hideText();
}

void DecodeTrace::hover_point_changed()
{
	QPoint hp = _view->hover_point();
	QString ann = get_annotation_at_point(hp);

	assert(_view);
	assert(_row_height);

	if (ann.isEmpty()) {
		hide_hover_annotation();
		return;
	}

	const int hover_row = get_row_at_point(hp);

	QFontMetrics m(QToolTip::font());
	const QRect text_size = m.boundingRect(QRect(), 0, ann);

	// This is OS-specific and unfortunately we can't query it, so
	// use an approximation to at least try to minimize the error.
	const int padding = 8;

	// Make sure the tool tip doesn't overlap with the mouse cursor.
	// If it did, the tool tip would constantly hide and re-appear.
	// We also push it up by one row so that it appears above the
	// decode trace, not below.
	hp.setX(hp.x() - (text_size.width() / 2) - padding);

	hp.setY(get_y() - (_row_height / 2) + (hover_row * _row_height)
		- _row_height - text_size.height());

	QToolTip::showText(_view->viewport()->mapToGlobal(hp), ann);
}

void DecodeTrace::create_decoder_form(int index,
	shared_ptr<data::decode::Decoder> &dec, QWidget *parent,
	QFormLayout *form)
{
	const GSList *l;

	assert(dec);
	const srd_decoder *const decoder = dec->decoder();
	assert(decoder);

	pv::widgets::DecoderGroupBox *const group =
		new pv::widgets::DecoderGroupBox(
			QString::fromUtf8(decoder->name));
	group->set_decoder_visible(dec->shown());

	_delete_mapper.setMapping(group, index);
	connect(group, SIGNAL(delete_decoder()), &_delete_mapper, SLOT(map()));

	_show_hide_mapper.setMapping(group, index);
	connect(group, SIGNAL(show_hide_decoder()),
		&_show_hide_mapper, SLOT(map()));

	QFormLayout *const decoder_form = new QFormLayout;
	group->add_layout(decoder_form);

	// Add the mandatory channels
	for(l = decoder->channels; l; l = l->next) {
		const struct srd_channel *const pdch =
			(struct srd_channel *)l->data;
		QComboBox *const combo = create_channel_selector(parent, dec, pdch);
		connect(combo, SIGNAL(currentIndexChanged(int)),
			this, SLOT(on_channel_selected(int)));
		decoder_form->addRow(tr("<b>%1</b> (%2) *")
			.arg(QString::fromUtf8(pdch->name))
			.arg(QString::fromUtf8(pdch->desc)), combo);

		const ChannelSelector s = {combo, dec, pdch};
		_channel_selectors.push_back(s);
	}

	// Add the optional channels
	for(l = decoder->opt_channels; l; l = l->next) {
		const struct srd_channel *const pdch =
			(struct srd_channel *)l->data;
		QComboBox *const combo = create_channel_selector(parent, dec, pdch);
		connect(combo, SIGNAL(currentIndexChanged(int)),
			this, SLOT(on_channel_selected(int)));
		decoder_form->addRow(tr("<b>%1</b> (%2)")
			.arg(QString::fromUtf8(pdch->name))
			.arg(QString::fromUtf8(pdch->desc)), combo);

		const ChannelSelector s = {combo, dec, pdch};
		_channel_selectors.push_back(s);
	}

	// Add the options
	shared_ptr<prop::binding::DecoderOptions> binding(
		new prop::binding::DecoderOptions(_decoder_stack, dec));
	binding->add_properties_to_form(decoder_form, true);

	_bindings.push_back(binding);

	form->addRow(group);
	_decoder_forms.push_back(group);
}

QComboBox* DecodeTrace::create_channel_selector(
	QWidget *parent, const shared_ptr<data::decode::Decoder> &dec,
	const srd_channel *const pdch)
{
	assert(dec);

	const vector< shared_ptr<Signal> > sigs = _session.get_signals();

	assert(_decoder_stack);
	const auto channel_iter = dec->channels().find(pdch);

	QComboBox *selector = new QComboBox(parent);

	selector->addItem("-", qVariantFromValue((void*)NULL));

	if (channel_iter == dec->channels().end())
		selector->setCurrentIndex(0);

	for(size_t i = 0; i < sigs.size(); i++) {
		const shared_ptr<view::Signal> s(sigs[i]);
		assert(s);

		if (dynamic_pointer_cast<LogicSignal>(s) && s->enabled())
		{
			selector->addItem(s->get_name(),
				qVariantFromValue((void*)s.get()));
			if ((*channel_iter).second == s)
				selector->setCurrentIndex(i + 1);
		}
	}

	return selector;
}

void DecodeTrace::commit_decoder_channels(shared_ptr<data::decode::Decoder> &dec)
{
	assert(dec);

	map<const srd_channel*, shared_ptr<LogicSignal> > channel_map;
	const vector< shared_ptr<Signal> > sigs = _session.get_signals();

	for (const ChannelSelector &s : _channel_selectors)
	{
		if(s._decoder != dec)
			break;

		const LogicSignal *const selection =
			(LogicSignal*)s._combo->itemData(
				s._combo->currentIndex()).value<void*>();

		for (shared_ptr<Signal> sig : sigs)
			if(sig.get() == selection) {
				channel_map[s._pdch] =
					dynamic_pointer_cast<LogicSignal>(sig);
				break;
			}
	}

	dec->set_channels(channel_map);
}

void DecodeTrace::commit_channels()
{
	assert(_decoder_stack);
	for (shared_ptr<data::decode::Decoder> dec : _decoder_stack->stack())
		commit_decoder_channels(dec);

	_decoder_stack->begin_decode();
}

void DecodeTrace::on_new_decode_data()
{
	if (_view)
		_view->update_viewport();
}

void DecodeTrace::delete_pressed()
{
	on_delete();
}

void DecodeTrace::on_delete()
{
	_session.remove_decode_signal(this);
}

void DecodeTrace::on_channel_selected(int)
{
	commit_channels();
}

void DecodeTrace::on_stack_decoder(srd_decoder *decoder)
{
	assert(decoder);
	assert(_decoder_stack);
	_decoder_stack->push(shared_ptr<data::decode::Decoder>(
		new data::decode::Decoder(decoder)));
	_decoder_stack->begin_decode();

	create_popup_form();
}

void DecodeTrace::on_delete_decoder(int index)
{
	_decoder_stack->remove(index);

	// Update the popup
	create_popup_form();	

	_decoder_stack->begin_decode();
}

void DecodeTrace::on_show_hide_decoder(int index)
{
	using pv::data::decode::Decoder;

	const list< shared_ptr<Decoder> > stack(_decoder_stack->stack());

	// Find the decoder in the stack
	auto iter = stack.cbegin();
	for(int i = 0; i < index; i++, iter++)
		assert(iter != stack.end());

	shared_ptr<Decoder> dec = *iter;
	assert(dec);

	const bool show = !dec->shown();
	dec->show(show);

	assert(index < (int)_decoder_forms.size());
	_decoder_forms[index]->set_decoder_visible(show);

	_view->update_viewport();
}

} // namespace view
} // namespace pv
