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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <extdef.h>

#include <cassert>
#include <cmath>

#include <QApplication>
#include <QFormLayout>
#include <QKeyEvent>
#include <QLineEdit>

#include "trace.hpp"
#include "tracepalette.hpp"
#include "view.hpp"

#include "pv/globalsettings.hpp"
#include "pv/widgets/colourbutton.hpp"
#include "pv/widgets/popup.hpp"

using std::pair;
using std::shared_ptr;

namespace pv {
namespace views {
namespace TraceView {

const QPen Trace::AxisPen(QColor(0, 0, 0, 30 * 256 / 100));
const int Trace::LabelHitPadding = 2;

const QColor Trace::BrightGrayBGColour = QColor(0, 0, 0, 10 * 255 / 100);
const QColor Trace::DarkGrayBGColour = QColor(0, 0, 0, 15 * 255 / 100);

Trace::Trace(shared_ptr<data::SignalBase> channel) :
	base_(channel),
	popup_(nullptr),
	popup_form_(nullptr)
{
	connect(channel.get(), SIGNAL(name_changed(const QString&)),
		this, SLOT(on_name_changed(const QString&)));
	connect(channel.get(), SIGNAL(colour_changed(const QColor&)),
		this, SLOT(on_colour_changed(const QColor&)));
}

shared_ptr<data::SignalBase> Trace::base() const
{
	return base_;
}

void Trace::paint_label(QPainter &p, const QRect &rect, bool hover)
{
	const int y = get_visual_y();

	p.setBrush(base_->colour());

	if (!enabled())
		return;

	const QRectF r = label_rect(rect);

	// When selected, move the arrow to the left so that the border can show
	const QPointF offs = (selected()) ? QPointF(-2, 0) : QPointF(0, 0);

	// Paint the label
	const float label_arrow_length = r.height() / 2;
	QPointF points[] = {
		offs + r.topLeft(),
		offs + QPointF(r.right() - label_arrow_length, r.top()),
		offs + QPointF(r.right(), y),
		offs + QPointF(r.right() - label_arrow_length, r.bottom()),
		offs + r.bottomLeft()
	};
	QPointF highlight_points[] = {
		offs + QPointF(r.left() + 1, r.top() + 1),
		offs + QPointF(r.right() - label_arrow_length, r.top() + 1),
		offs + QPointF(r.right() - 1, y),
		offs + QPointF(r.right() - label_arrow_length, r.bottom() - 1),
		offs + QPointF(r.left() + 1, r.bottom() - 1)
	};

	if (selected()) {
		p.setPen(highlight_pen());
		p.setBrush(Qt::transparent);
		p.drawPolygon(points, countof(points));
	}

	p.setPen(Qt::transparent);
	p.setBrush(hover ? base_->colour().lighter() : base_->colour());
	p.drawPolygon(points, countof(points));

	p.setPen(base_->colour().lighter());
	p.setBrush(Qt::transparent);
	p.drawPolygon(highlight_points, countof(highlight_points));

	p.setPen(base_->colour().darker());
	p.setBrush(Qt::transparent);
	p.drawPolygon(points, countof(points));

	// Paint the text
	p.setPen(select_text_colour(base_->colour()));
	p.setFont(QApplication::font());
	p.drawText(QRectF(r.x(), r.y(),
		r.width() - label_arrow_length, r.height()),
		Qt::AlignCenter | Qt::AlignVCenter, base_->name());
}

QMenu* Trace::create_context_menu(QWidget *parent)
{
	QMenu *const menu = ViewItem::create_context_menu(parent);

	return menu;
}

pv::widgets::Popup* Trace::create_popup(QWidget *parent)
{
	using pv::widgets::Popup;

	popup_ = new Popup(parent);
	popup_->set_position(parent->mapToGlobal(
		point(parent->rect())), Popup::Right);

	create_popup_form();

	connect(popup_, SIGNAL(closed()), this, SLOT(on_popup_closed()));

	return popup_;
}

QRectF Trace::label_rect(const QRectF &rect) const
{
	QFontMetrics m(QApplication::font());
	const QSize text_size(
		m.boundingRect(QRect(), 0, base_->name()).width(), m.height());
	const QSizeF label_size(
		text_size.width() + LabelPadding.width() * 2,
		ceilf((text_size.height() + LabelPadding.height() * 2) / 2) * 2);
	const float half_height = label_size.height() / 2;
	return QRectF(
		rect.right() - half_height - label_size.width() - 0.5,
		get_visual_y() + 0.5f - half_height,
		label_size.width() + half_height,
		label_size.height());
}

void Trace::paint_back(QPainter &p, ViewItemPaintParams &pp)
{
	const View *view = owner_->view();
	assert(view);

	if (view->coloured_bg())
		p.setBrush(base_->bgcolour());
	else
		p.setBrush(pp.next_bg_colour_state() ? BrightGrayBGColour : DarkGrayBGColour);

	p.setPen(QPen(Qt::NoPen));

	const pair<int, int> extents = v_extents();
	p.drawRect(pp.left(), get_visual_y() + extents.first,
		pp.width(), extents.second - extents.first);
}

void Trace::paint_axis(QPainter &p, ViewItemPaintParams &pp, int y)
{
	p.setRenderHint(QPainter::Antialiasing, false);

	p.setPen(AxisPen);
	p.drawLine(QPointF(pp.left(), y), QPointF(pp.right(), y));

	p.setRenderHint(QPainter::Antialiasing, true);
}

void Trace::add_colour_option(QWidget *parent, QFormLayout *form)
{
	using pv::widgets::ColourButton;

	ColourButton *const colour_button = new ColourButton(
		TracePalette::Rows, TracePalette::Cols, parent);
	colour_button->set_palette(TracePalette::Colours);
	colour_button->set_colour(base_->colour());
	connect(colour_button, SIGNAL(selected(const QColor&)),
		this, SLOT(on_colouredit_changed(const QColor&)));

	form->addRow(tr("Colour"), colour_button);
}

void Trace::create_popup_form()
{
	// Clear the layout

	// Transfer the layout and the child widgets to a temporary widget
	// which we delete after the event was handled. This way, the layout
	// and all widgets contained therein are deleted after the event was
	// handled, leaving the parent popup_ time to handle the change.
	if (popup_form_) {
		QWidget *suicidal = new QWidget();
		suicidal->setLayout(popup_form_);
		suicidal->deleteLater();
	}

	// Repopulate the popup
	popup_form_ = new QFormLayout(popup_);
	popup_->setLayout(popup_form_);
	populate_popup_form(popup_, popup_form_);
}

void Trace::populate_popup_form(QWidget *parent, QFormLayout *form)
{
	QLineEdit *const name_edit = new QLineEdit(parent);
	name_edit->setText(base_->name());
	connect(name_edit, SIGNAL(textChanged(const QString&)),
		this, SLOT(on_nameedit_changed(const QString&)));
	form->addRow(tr("Name"), name_edit);

	add_colour_option(parent, form);
}

void Trace::set_name(QString name)
{
	base_->set_name(name);
}

void Trace::set_colour(QColor colour)
{
	base_->set_colour(colour);
}

void Trace::on_name_changed(const QString &text)
{
	/* This event handler is called by SignalBase when the name was changed there */
	(void)text;

	if (owner_) {
		owner_->extents_changed(true, false);
		owner_->row_item_appearance_changed(true, false);
	}
}

void Trace::on_colour_changed(const QColor &colour)
{
	/* This event handler is called by SignalBase when the colour was changed there */
	(void)colour;

	if (owner_)
		owner_->row_item_appearance_changed(true, true);
}

void Trace::on_popup_closed()
{
	popup_ = nullptr;
	popup_form_ = nullptr;
}

void Trace::on_nameedit_changed(const QString &name)
{
	/* This event handler notifies SignalBase that the name changed */
	set_name(name);
}

void Trace::on_colouredit_changed(const QColor &colour)
{
	/* This event handler notifies SignalBase that the colour changed */
	set_colour(colour);
}

} // namespace TraceView
} // namespace views
} // namespace pv
