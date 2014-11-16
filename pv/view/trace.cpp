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

#include <QApplication>
#include <QFormLayout>
#include <QKeyEvent>
#include <QLineEdit>

#include "trace.h"
#include "tracepalette.h"
#include "view.h"

#include <pv/widgets/colourbutton.h>
#include <pv/widgets/popup.h>

namespace pv {
namespace view {

const QPen Trace::AxisPen(QColor(128, 128, 128, 64));
const int Trace::LabelHitPadding = 2;

Trace::Trace(QString name) :
	_name(name),
	_popup(NULL),
	_popup_form(NULL)
{
}

QString Trace::name() const
{
	return _name;
}

void Trace::set_name(QString name)
{
	_name = name;
}

QColor Trace::colour() const
{
	return _colour;
}

void Trace::set_colour(QColor colour)
{
	_colour = colour;
}

void Trace::paint_label(QPainter &p, int right, bool hover)
{
	const int y = get_y();

	p.setBrush(_colour);

	if (!enabled())
		return;

	const QRectF r = label_rect(right);

	// Paint the label
	const float label_arrow_length = r.height() / 2;
	const QPointF points[] = {
		r.topLeft(),
		QPointF(r.right() - label_arrow_length, r.top()),
		QPointF(r.right(), y),
		QPointF(r.right() - label_arrow_length, r.bottom()),
		r.bottomLeft()
	};
	const QPointF highlight_points[] = {
		QPointF(r.left() + 1, r.top() + 1),
		QPointF(r.right() - label_arrow_length, r.top() + 1),
		QPointF(r.right() - 1, y),
		QPointF(r.right() - label_arrow_length, r.bottom() - 1),
		QPointF(r.left() + 1, r.bottom() - 1)
	};

	if (selected()) {
		p.setPen(highlight_pen());
		p.setBrush(Qt::transparent);
		p.drawPolygon(points, countof(points));
	}

	p.setPen(Qt::transparent);
	p.setBrush(hover ? _colour.lighter() : _colour);
	p.drawPolygon(points, countof(points));

	p.setPen(_colour.lighter());
	p.setBrush(Qt::transparent);
	p.drawPolygon(highlight_points, countof(highlight_points));

	p.setPen(_colour.darker());
	p.setBrush(Qt::transparent);
	p.drawPolygon(points, countof(points));

	// Paint the text
	p.setPen(get_text_colour());
	p.setFont(QApplication::font());
	p.drawText(QRectF(r.x(), r.y(),
		r.width() - label_arrow_length, r.height()),
		Qt::AlignCenter | Qt::AlignVCenter, _name);
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

	create_popup_form();

	connect(_popup, SIGNAL(closed()),
		this, SLOT(on_popup_closed()));

	return _popup;
}

QRectF Trace::label_rect(int right)
{
	using pv::view::View;

	QFontMetrics m(QApplication::font());
	const QSize text_size(
		m.boundingRect(QRect(), 0, _name).width(),
		m.boundingRect(QRect(), 0, "Tg").height());
	const QSizeF label_size(
		text_size.width() + View::LabelPadding.width() * 2,
		ceilf((text_size.height() + View::LabelPadding.height() * 2) / 2) * 2);
	const float half_height = label_size.height() / 2;
	return QRectF(
		right - half_height - label_size.width() - 0.5,
		get_y() + 0.5f - half_height,
		label_size.width() + half_height,
		label_size.height());
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

void Trace::create_popup_form()
{
	// Clear the layout

	// Transfer the layout and the child widgets to a temporary widget
	// which then goes out of scope destroying the layout and all the child
	// widgets.
	if (_popup_form)
		QWidget().setLayout(_popup_form);

	// Repopulate the popup
	_popup_form = new QFormLayout(_popup);
	_popup->setLayout(_popup_form);
	populate_popup_form(_popup, _popup_form);
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
