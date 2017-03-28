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

#ifndef PULSEVIEW_PV_VIEWS_TRACEVIEW_DECODETRACE_HPP
#define PULSEVIEW_PV_VIEWS_TRACEVIEW_DECODETRACE_HPP

#include "trace.hpp"

#include <list>
#include <map>
#include <memory>
#include <vector>

#include <QSignalMapper>

#include <pv/binding/decoder.hpp>
#include <pv/data/decode/row.hpp>
#include <pv/data/signalbase.hpp>

using std::list;
using std::map;
using std::pair;
using std::shared_ptr;
using std::vector;

struct srd_channel;
struct srd_decoder;

class QComboBox;

namespace pv {

class Session;

namespace data {
class DecoderStack;
class SignalBase;

namespace decode {
class Annotation;
class Decoder;
class Row;
}
}

namespace widgets {
class DecoderGroupBox;
}

namespace views {
namespace TraceView {

class DecodeTrace : public Trace
{
	Q_OBJECT

private:
	struct ChannelSelector
	{
		const QComboBox *combo_;
		const shared_ptr<pv::data::decode::Decoder> decoder_;
		const srd_channel *pdch_;
	};

private:
	static const QColor DecodeColours[4];
	static const QColor ErrorBgColour;
	static const QColor NoDecodeColour;

	static const int ArrowSize;
	static const double EndCapWidth;
	static const int RowTitleMargin;
	static const int DrawPadding;

	static const QColor Colours[16];
	static const QColor OutlineColours[16];

public:
	DecodeTrace(pv::Session &session, shared_ptr<data::SignalBase> signalbase,
		int index);

	bool enabled() const;

	const shared_ptr<pv::data::DecoderStack>& decoder() const;

	shared_ptr<data::SignalBase> base() const;

	/**
	 * Computes the vertical extents of the contents of this row item.
	 * @return A pair containing the minimum and maximum y-values.
	 */
	pair<int, int> v_extents() const;

	/**
	 * Paints the background layer of the trace with a QPainter
	 * @param p the QPainter to paint into.
	 * @param pp the painting parameters object to paint with..
	 */
	void paint_back(QPainter &p, const ViewItemPaintParams &pp);

	/**
	 * Paints the mid-layer of the trace with a QPainter
	 * @param p the QPainter to paint into.
	 * @param pp the painting parameters object to paint with.
	 */
	void paint_mid(QPainter &p, const ViewItemPaintParams &pp);

	/**
	 * Paints the foreground layer of the trace with a QPainter
	 * @param p the QPainter to paint into.
	 * @param pp the painting parameters object to paint with.
	 */
	void paint_fore(QPainter &p, const ViewItemPaintParams &pp);

	void populate_popup_form(QWidget *parent, QFormLayout *form);

	QMenu* create_context_menu(QWidget *parent);

	void delete_pressed();

private:
	void draw_annotations(vector<pv::data::decode::Annotation> annotations,
		QPainter &p, int h, const ViewItemPaintParams &pp, int y,
		size_t base_colour, int row_title_width);

	void draw_annotation(const pv::data::decode::Annotation &a, QPainter &p,
		int h, const ViewItemPaintParams &pp, int y,
		size_t base_colour, int row_title_width) const;

	void draw_annotation_block(vector<pv::data::decode::Annotation> annotations,
		QPainter &p, int h, int y, size_t base_colour) const;

	void draw_instant(const pv::data::decode::Annotation &a, QPainter &p,
		int h, double x, int y) const;

	void draw_range(const pv::data::decode::Annotation &a, QPainter &p,
		int h, double start, double end, int y, const ViewItemPaintParams &pp,
		int row_title_width) const;

	void draw_error(QPainter &p, const QString &message,
		const ViewItemPaintParams &pp);

	void draw_unresolved_period(QPainter &p, int h, int left,
		int right) const;

	pair<double, double> get_pixels_offset_samples_per_pixel() const;

	/**
	 * Determines the start and end sample for a given pixel range.
	 * @param x_start the X coordinate of the start sample in the view
	 * @param x_end the X coordinate of the end sample in the view
	 * @return Returns a pair containing the start sample and the end
	 * 	sample that correspond to the start and end coordinates.
	 */
	pair<uint64_t, uint64_t> get_sample_range(int x_start, int x_end) const;

	int get_row_at_point(const QPoint &point);

	const QString get_annotation_at_point(const QPoint &point);

	void create_decoder_form(int index,
		shared_ptr<pv::data::decode::Decoder> &dec,
		QWidget *parent, QFormLayout *form);

	QComboBox* create_channel_selector(QWidget *parent,
		const shared_ptr<pv::data::decode::Decoder> &dec,
		const srd_channel *const pdch);

	void commit_decoder_channels(shared_ptr<data::decode::Decoder> &dec);

	void commit_channels();

public:
	void hover_point_changed();

private Q_SLOTS:
	void on_new_decode_data();

	void on_delete();

	void on_channel_selected(int);

	void on_stack_decoder(srd_decoder *decoder);

	void on_delete_decoder(int index);

	void on_show_hide_decoder(int index);

private:
	pv::Session &session_;

	vector<data::decode::Row> visible_rows_;
	uint64_t decode_start_, decode_end_;

	list< shared_ptr<pv::binding::Decoder> > bindings_;

	list<ChannelSelector> channel_selectors_;
	vector<pv::widgets::DecoderGroupBox*> decoder_forms_;

	map<data::decode::Row, int> row_title_widths_;
	int row_height_, max_visible_rows_;

	int min_useful_label_width_;

	QSignalMapper delete_mapper_, show_hide_mapper_;
};

} // namespace TraceView
} // namespace views
} // namespace pv

#endif // PULSEVIEW_PV_VIEWS_TRACEVIEW_DECODETRACE_HPP
