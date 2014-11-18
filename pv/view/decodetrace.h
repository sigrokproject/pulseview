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

#include "trace.h"

#include <list>
#include <map>
#include <memory>

#include <QSignalMapper>

#include <pv/prop/binding/decoderoptions.h>
#include <pv/data/decode/row.h>

struct srd_channel;
struct srd_decoder;

class QComboBox;

namespace pv {

class SigSession;

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
		const QComboBox *_combo;
		const std::shared_ptr<pv::data::decode::Decoder> _decoder;
		const srd_channel *_pdch;
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
	DecodeTrace(pv::SigSession &session,
		std::shared_ptr<pv::data::DecoderStack> decoder_stack,
		int index);

	bool enabled() const;

	const std::shared_ptr<pv::data::DecoderStack>& decoder() const;

	void set_view(pv::view::View *view);

	/**
	 * Paints the background layer of the trace with a QPainter
	 * @param p the QPainter to paint into.
	 * @param left the x-coordinate of the left edge of the signal.
	 * @param right the x-coordinate of the right edge of the signal.
	 **/
	void paint_back(QPainter &p, int left, int right);

	/**
	 * Paints the mid-layer of the trace with a QPainter
	 * @param p the QPainter to paint into.
	 * @param left the x-coordinate of the left edge of the signal
	 * @param right the x-coordinate of the right edge of the signal
	 **/
	void paint_mid(QPainter &p, int left, int right);

	/**
	 * Paints the foreground layer of the trace with a QPainter
	 * @param p the QPainter to paint into.
	 * @param left the x-coordinate of the left edge of the signal
	 * @param right the x-coordinate of the right edge of the signal
	 **/
	void paint_fore(QPainter &p, int left, int right);

	void populate_popup_form(QWidget *parent, QFormLayout *form);

	QMenu* create_context_menu(QWidget *parent);

	void delete_pressed();

private:
	void draw_annotation(const pv::data::decode::Annotation &a, QPainter &p,
		QColor text_colour, int text_height, int left, int right, int y,
		size_t base_colour) const;

	void draw_instant(const pv::data::decode::Annotation &a, QPainter &p,
		QColor fill, QColor outline, QColor text_color, int h, double x,
		int y) const;

	void draw_range(const pv::data::decode::Annotation &a, QPainter &p,
		QColor fill, QColor outline, QColor text_color, int h, double start,
		double end, int y) const;

	void draw_error(QPainter &p, const QString &message,
		int left, int right);

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

	void hide_hover_annotation();

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
	pv::SigSession &_session;
	std::shared_ptr<pv::data::DecoderStack> _decoder_stack;

	uint64_t _decode_start, _decode_end;

	std::list< std::shared_ptr<pv::prop::binding::DecoderOptions> >
		_bindings;

	std::list<ChannelSelector> _channel_selectors;
	std::vector<pv::widgets::DecoderGroupBox*> _decoder_forms;

	std::vector<data::decode::Row> _visible_rows;
	int _text_height, _row_height;

	QSignalMapper _delete_mapper, _show_hide_mapper;
};

} // namespace view
} // namespace pv

#endif // PULSEVIEW_PV_VIEW_DECODETRACE_H
