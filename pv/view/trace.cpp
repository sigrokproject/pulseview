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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <extdef.h>

#include <assert.h>
#include <math.h>

#include <QFormLayout>
#include <QLineEdit>

#include "trace.h"
#include "tracepalette.h"
#include "view.h"

#include <pv/widgets/colourbutton.h>

namespace pv {
namespace view {

const QPen Trace::AxisPen(QColor(128, 128, 128, 64));
const int Trace::LabelHitPadding = 2;

Trace::Trace(pv::SigSession &session, QString name) :
	_session(session),
	_name(name),
	_v_offset(0),
	_popup(NULL),
	_popup_form(NULL)
{
}

QString Trace::get_name() const
{
	return _name;
}

void Trace::set_name(QString name)
{
	_name = name;
}

QColor Trace::get_colour() const
{
	return _colour;
}

void Trace::set_colour(QColor colour)
{
	_colour = colour;
}

int Trace::get_v_offset() const
{
	return _v_offset;
}

void Trace::set_v_offset(int v_offset)
{
	_v_offset = v_offset;
}

void Trace::set_view(pv::view::View *view)
{
	assert(view);
	_view = view;
}

void Trace::paint_back(QPainter &p, int left, int right)
{
	(void)p;
	(void)left;
	(void)right;
}

void Trace::paint_mid(QPainter &p, int left, int right)
{
	(void)p;
	(void)left;
	(void)right;
}

void Trace::paint_fore(QPainter &p, int left, int right)
{
	(void)p;
	(void)left;
	(void)right;
}

void Trace::paint_label(QPainter &p, int right, bool hover)
{
	assert(_view);
	const int y = _v_offset - _view->v_offset();

	p.setBrush(_colour);

	if (!enabled())
		return;

	const QColor colour = get_colour();

	compute_text_size(p);
	const QRectF label_rect = get_label_rect(right);

	// Paint the label
	const QPointF points[] = {
		label_rect.topLeft(),
		label_rect.topRight(),
		QPointF(right, y),
		label_rect.bottomRight(),
		label_rect.bottomLeft()
	};

	const QPointF highlight_points[] = {
		QPointF(label_rect.left() + 1, label_rect.top() + 1),
		QPointF(label_rect.right(), label_rect.top() + 1),
		QPointF(right - 1, y),
		QPointF(label_rect.right(), label_rect.bottom() - 1),
		QPointF(label_rect.left() + 1, label_rect.bottom() - 1)
	};

	if (selected()) {
		p.setPen(highlight_pen());
		p.setBrush(Qt::transparent);
		p.drawPolygon(points, countof(points));
	}

	p.setPen(Qt::transparent);
	p.setBrush(hover ? colour.lighter() : colour);
	p.drawPolygon(points, countof(points));

	p.setPen(colour.lighter());
	p.setBrush(Qt::transparent);
	p.drawPolygon(highlight_points, countof(highlight_points));

	p.setPen(colour.darker());
	p.setBrush(Qt::transparent);
	p.drawPolygon(points, countof(points));

	// Paint the text
	p.setPen(get_text_colour());
	p.drawText(label_rect, Qt::AlignCenter | Qt::AlignVCenter, _name);
}

bool Trace::pt_in_label_rect(int left, int right, const QPoint &point)
{
	(void)left;

	const QRectF label = get_label_rect(right);
	return QRectF(
		QPointF(label.left() - LabelHitPadding,
			label.top() - LabelHitPadding),
		QPointF(right, label.bottom() + LabelHitPadding)
			).contains(point);
}

QMenu* Trace::create_context_menu(QWidget *parent)
{
	QMenu *const menu = SelectableItem::create_context_menu(parent);

	return menu;
}

pv::widgets::Popup* Trace::create_popup(QWidget *parent)
{
	using pv::widgets::Popup;

	_popup = new Popup(parent);
	_popup_form = new QFormLayout(_popup);
	_popup->setLayout(_popup_form);

	populate_popup_form(_popup, _popup_form);

	connect(_popup, SIGNAL(closed()),
		this, SLOT(on_popup_closed()));

	return _popup;
}

int Trace::get_y() const
{
	return _v_offset - _view->v_offset();
}

QColor Trace::get_text_colour() const
{
	return (_colour.lightness() > 64) ? Qt::black : Qt::white;
}

void Trace::paint_axis(QPainter &p, int y, int left, int right)
{
	p.setPen(AxisPen);
	p.drawLine(QPointF(left, y + 0.5f), QPointF(right, y + 0.5f));
}

void Trace::add_colour_option(QWidget *parent, QFormLayout *form)
{
	using pv::widgets::ColourButton;

	ColourButton *const colour_button = new ColourButton(
		TracePalette::Rows, TracePalette::Cols, parent);
	colour_button->set_palette(TracePalette::Colours);
	colour_button->set_colour(_colour);
	connect(colour_button, SIGNAL(selected(const QColor&)),
		this, SLOT(on_colour_changed(const QColor&)));

	form->addRow(tr("Colour"), colour_button);
}

void Trace::populate_popup_form(QWidget *parent, QFormLayout *form)
{
	QLineEdit *const name_edit = new QLineEdit(parent);
	name_edit->setText(_name);
	connect(name_edit, SIGNAL(textChanged(const QString&)),
		this, SLOT(on_text_changed(const QString&)));
	form->addRow(tr("Name"), name_edit);

	add_colour_option(parent, form);
}

void Trace::compute_text_size(QPainter &p)
{
	_text_size = QSize(
		p.boundingRect(QRectF(), 0, _name).width(),
		p.boundingRect(QRectF(), 0, "Tg").height());
}

QRectF Trace::get_label_rect(int right)
{
	using pv::view::View;

	assert(_view);

	const QSizeF label_size(
		_text_size.width() + View::LabelPadding.width() * 2,
		ceilf((_text_size.height() + View::LabelPadding.height() * 2) / 2) * 2);
	const float label_arrow_length = label_size.height() / 2;
	return QRectF(
		right - label_arrow_length - label_size.width() - 0.5,
		get_y() + 0.5f - label_size.height() / 2,
		label_size.width(), label_size.height());
}

void Trace::on_popup_closed()
{
	_popup = NULL;
	_popup_form = NULL;
}

void Trace::on_text_changed(const QString &text)
{
	set_name(text);
	text_changed();
}

void Trace::on_colour_changed(const QColor &colour)
{
	set_colour(colour);
	colour_changed();
}

} // namespace view
} // namespace pv
