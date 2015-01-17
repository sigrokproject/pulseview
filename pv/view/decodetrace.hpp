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

#ifndef PULSEVIEW_PV_VIEW_DECODETRACE_H
#define PULSEVIEW_PV_VIEW_DECODETRACE_H

#include "trace.hpp"

#include <list>
#include <map>
#include <memory>

#include <QSignalMapper>

#include <pv/binding/decoderoptions.hpp>
#include <pv/data/decode/row.hpp>

struct srd_channel;
struct srd_decoder;

class QComboBox;

namespace pv {

class Session;

namespace data {
class DecoderStack;

namespace decode {
class Annotation;
class Decoder;
class Row;
}
}

namespace widgets {
class DecoderGroupBox;
}

namespace view {

class DecodeTrace : public Trace
{
	Q_OBJECT

private:
	struct ChannelSelector
	{
		const QComboBox *combo_;
		const std::shared_ptr<pv::data::decode::Decoder> decoder_;
		const srd_channel *pdch_;
	};

private:
	static const QColor DecodeColours[4];
	static const QColor ErrorBgColour;
	static const QColor NoDecodeColour;

	static const int ArrowSize;
	static const double EndCapWidth;
	static const int DrawPadding;

	static const QColor Colours[16];
	static const QColor OutlineColours[16];

public:
	DecodeTrace(pv::Session &session,
		std::shared_ptr<pv::data::DecoderStack> decoder_stack,
		int index);

	bool enabled() const;

	const std::shared_ptr<pv::data::DecoderStack>& decoder() const;

	/**
	 * Computes the vertical extents of the contents of this row item.
	 * @return A pair containing the minimum and maximum y-values.
	 */
	std::pair<int, int> v_extents() const;

	/**
	 * Paints the background layer of the trace with a QPainter
	 * @param p the QPainter to paint into.
	 * @param pp the painting parameters object to paint with..
	 **/
	void paint_back(QPainter &p, const ViewItemPaintParams &pp);

	/**
	 * Paints the mid-layer of the trace with a QPainter
	 * @param p the QPainter to paint into.
	 * @param pp the painting parameters object to paint with.
	 **/
	void paint_mid(QPainter &p, const ViewItemPaintParams &pp);

	/**
	 * Paints the foreground layer of the trace with a QPainter
	 * @param p the QPainter to paint into.
	 * @param pp the painting parameters object to paint with.
	 **/
	void paint_fore(QPainter &p, const ViewItemPaintParams &pp);

	void populate_popup_form(QWidget *parent, QFormLayout *form);

	QMenu* create_context_menu(QWidget *parent);

	void delete_pressed();

private:
	void draw_annotation(const pv::data::decode::Annotation &a, QPainter &p,
		int text_height, const ViewItemPaintParams &pp, int y,
		size_t base_colour) const;

	void draw_instant(const pv::data::decode::Annotation &a, QPainter &p,
		QColor fill, QColor outline, int h, double x, int y) const;

	void draw_range(const pv::data::decode::Annotation &a, QPainter &p,
		QColor fill, QColor outline, int h, double start,
		double end, int y) const;

	void draw_error(QPainter &p, const QString &message,
		const ViewItemPaintParams &pp);

	void draw_unresolved_period(QPainter &p, int h, int left,
		int right) const;

	std::pair<double, double> get_pixels_offset_samples_per_pixel() const;

	/**
	 * Determines the start and end sample for a given pixel range.
	 * @param x_start the X coordinate of the start sample in the view
	 * @param x_end the X coordinate of the end sample in the view
	 * @return Returns a pair containing the start sample and the end
	 * 	sample that correspond to the start and end coordinates.
	 */
	std::pair<uint64_t, uint64_t> get_sample_range(int x_start, int x_end) const;

	int get_row_at_point(const QPoint &point);

	const QString get_annotation_at_point(const QPoint &point);

	void create_decoder_form(int index,
		std::shared_ptr<pv::data::decode::Decoder> &dec,
		QWidget *parent, QFormLayout *form);

	QComboBox* create_channel_selector(QWidget *parent,
		const std::shared_ptr<pv::data::decode::Decoder> &dec,
		const srd_channel *const pdch);

	void commit_decoder_channels(
		std::shared_ptr<data::decode::Decoder> &dec);

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
	std::shared_ptr<pv::data::DecoderStack> decoder_stack_;

	uint64_t decode_start_, decode_end_;

	std::list< std::shared_ptr<pv::binding::DecoderOptions> >
		bindings_;

	std::list<ChannelSelector> channel_selectors_;
	std::vector<pv::widgets::DecoderGroupBox*> decoder_forms_;

	std::vector<data::decode::Row> visible_rows_;
	int row_height_;

	QSignalMapper delete_mapper_, show_hide_mapper_;
};

} // namespace view
} // namespace pv

#endif // PULSEVIEW_PV_VIEW_DECODETRACE_H
