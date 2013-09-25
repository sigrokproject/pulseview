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

#include <QColorDialog>
#include <QInputDialog>

#include "trace.h"
#include "view.h"

#include <pv/widgets/popup.h>

namespace pv {
namespace view {

const QPen Trace::AxisPen(QColor(128, 128, 128, 64));
const int Trace::LabelHitPadding = 2;

Trace::Trace(pv::SigSession &session, QString name) :
	_session(session),
	_name(name),
	_v_offset(0)
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

	QAction *const set_name = new QAction(tr("Set &Name..."), this);
	connect(set_name, SIGNAL(triggered()),
		this, SLOT(on_action_set_name_triggered()));
	menu->addAction(set_name);

	QAction *const set_colour = new QAction(tr("Set &Colour..."), this);
	connect(set_colour, SIGNAL(triggered()),
		this, SLOT(on_action_set_colour_triggered()));
	menu->addAction(set_colour);

	return menu;
}

pv::widgets::Popup* Trace::create_popup(QWidget *parent)
{
	using pv::widgets::Popup;
	Popup *const popup = new Popup(parent);
	QFormLayout *const form = new QFormLayout(popup);
	popup->setLayout(form);
	populate_popup_form(popup, form);
	return popup;
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

void Trace::populate_popup_form(QWidget *parent, QFormLayout *form)
{
	form->addRow("Name", new QLineEdit(parent));
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

void Trace::on_action_set_name_triggered()
{
	bool ok = false;

	const QString new_label = QInputDialog::getText(_context_parent,
		tr("Set Name"), tr("Name"), QLineEdit::Normal, get_name(),
		&ok);

	if (ok)
		set_name(new_label);
}

void Trace::on_action_set_colour_triggered()
{
	const QColor new_colour = QColorDialog::getColor(
		get_colour(), _context_parent, tr("Set Colour"));

	if (new_colour.isValid())
		set_colour(new_colour);
}


} // namespace view
} // namespace pv
