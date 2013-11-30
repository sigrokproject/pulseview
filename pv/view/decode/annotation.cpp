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

#include <algorithm>

#include <boost/foreach.hpp>

#include <QPainter>

#include "annotation.h"

using namespace std;

namespace pv {
namespace view {
namespace decode {

const double Annotation::EndCapWidth = 5;
const int Annotation::DrawPadding = 100;

const QColor Annotation::Colours[7] = {
	QColor(0xFC, 0xE9, 0x4F),	// Light Butter
	QColor(0xFC, 0xAF, 0x3E),	// Light Orange
	QColor(0xE9, 0xB9, 0x6E),	// Light Chocolate
	QColor(0x8A, 0xE2, 0x34),	// Light Green
	QColor(0x72, 0x9F, 0xCF),	// Light Blue
	QColor(0xAD, 0x7F, 0xA8),	// Light Plum
	QColor(0xEF, 0x29, 0x29)	// Light Red
};

Annotation::Annotation(const srd_proto_data *const pdata) :
	_start_sample(pdata->start_sample),
	_end_sample(pdata->end_sample)
{
	assert(pdata);
	const srd_proto_data_annotation *const pda =
		(const srd_proto_data_annotation*)pdata->data;
	assert(pda);

	const char *const *annotations = (char**)pda->ann_text;
	while(*annotations) {
		_annotations.push_back(QString(*annotations));
		annotations++;
	}
}

void Annotation::paint(QPainter &p, QColor text_color, int h,
	int left, int right, double samples_per_pixel, double pixels_offset,
	int y)
{
	const double start = _start_sample / samples_per_pixel -
		pixels_offset;
	const double end = _end_sample / samples_per_pixel -
		pixels_offset;
	const QColor fill = Colours[(_format * (countof(Colours) / 2 + 1)) %
		countof(Colours)];
	const QColor outline(fill.darker());

	if (start > right + DrawPadding || end < left - DrawPadding)
		return;

	if (_start_sample == _end_sample)
		draw_instant(p, fill, outline, text_color, h,
			start, y);
	else
		draw_range(p, fill, outline, text_color, h,
			start, end, y);
}

void Annotation::draw_instant(QPainter &p, QColor fill, QColor outline,
	QColor text_color, int h, double x, int y)
{
	const QString text = _annotations.empty() ?
		QString() : _annotations.back();
	const double w = min(p.boundingRect(QRectF(), 0, text).width(),
		0.0) + h;
	const QRectF rect(x - w / 2, y - h / 2, w, h);

	p.setPen(outline);
	p.setBrush(fill);
	p.drawRoundedRect(rect, h / 2, h / 2);

	p.setPen(text_color);
	p.drawText(rect, Qt::AlignCenter | Qt::AlignVCenter, text);
}

void Annotation::draw_range(QPainter &p, QColor fill, QColor outline,
	QColor text_color, int h, double start, double end, int y)
{
	const double cap_width = min((end - start) / 2, EndCapWidth);

	QPointF pts[] = {
		QPointF(start, y + .5f),
		QPointF(start + cap_width, y + .5f - h / 2),
		QPointF(end - cap_width, y + .5f - h / 2),
		QPointF(end, y + .5f),
		QPointF(end - cap_width, y + .5f + h / 2),
		QPointF(start + cap_width, y + .5f + h / 2)
	};

	p.setPen(outline);
	p.setBrush(fill);
	p.drawConvexPolygon(pts, countof(pts));

	if (_annotations.empty())
		return;

	QRectF rect(start + cap_width, y - h / 2,
		end - start - cap_width * 2, h);
	p.setPen(text_color);

	// Try to find an annotation that will fit
	QString best_annotation;
	int best_width = 0;

	BOOST_FOREACH(QString &a, _annotations) {
		const int w = p.boundingRect(QRectF(), 0, a).width();
		if (w <= rect.width() && w > best_width)
			best_annotation = a, best_width = w;
	}

	if (best_annotation.isEmpty())
		best_annotation = _annotations.back();

	// If not ellide the last in the list
	p.drawText(rect, Qt::AlignCenter, p.fontMetrics().elidedText(
		best_annotation, Qt::ElideRight, rect.width()));
}

} // namespace decode
} // namespace view
} // namespace pv
