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

#include "viewitem.hpp"

#include <climits>

#include <QApplication>
#include <QMenu>
#include <QPalette>

namespace pv {
namespace view {

const QSizeF ViewItem::LabelPadding(4, 0);
const int ViewItem::HighlightRadius = 3;

ViewItem::ViewItem() :
	context_parent_(nullptr),
	drag_point_(INT_MIN, INT_MIN),
	selected_(false)
{
}

bool ViewItem::selected() const
{
	return selected_;
}

void ViewItem::select(bool select)
{
	selected_ = select;
}

bool ViewItem::is_draggable() const
{
	return true;
}

bool ViewItem::dragging() const
{
	return drag_point_.x() != INT_MIN && drag_point_.y() != INT_MIN;
}

void ViewItem::drag()
{
	if (is_draggable())
		drag_point_ = point(QRect());
}

void ViewItem::drag_release()
{
	drag_point_ = QPoint(INT_MIN, INT_MIN);
}

QRectF ViewItem::label_rect(const QRectF &rect) const
{
	(void)rect;
	return QRectF();
}

QRectF ViewItem::hit_box_rect(const ViewItemPaintParams &pp) const
{
	(void)pp;
	return QRectF();
}

QMenu* ViewItem::create_context_menu(QWidget *parent)
{
	context_parent_ = parent;
	return new QMenu(parent);
}

widgets::Popup* ViewItem::create_popup(QWidget *parent)
{
	(void)parent;
	return nullptr;
}

void ViewItem::delete_pressed()
{
}

QPen ViewItem::highlight_pen()
{
	return QPen(QApplication::palette().brush(
		QPalette::Highlight), HighlightRadius * 2,
		Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
}

void ViewItem::paint_label(QPainter &p, const QRect &rect, bool hover)
{
	(void)p;
	(void)rect;
	(void)hover;
}

void ViewItem::paint_back(QPainter &p, const ViewItemPaintParams &pp)
{
	(void)p;
	(void)pp;
}

void ViewItem::paint_mid(QPainter &p, const ViewItemPaintParams &pp)
{
	(void)p;
	(void)pp;
}

void ViewItem::paint_fore(QPainter &p, const ViewItemPaintParams &pp)
{
	(void)p;
	(void)pp;
}

QColor ViewItem::select_text_colour(QColor background)
{
	return (background.lightness() > 110) ? Qt::black : Qt::white;
}

} // namespace view
} // namespace pv
