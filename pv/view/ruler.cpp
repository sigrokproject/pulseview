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

#include <extdef.h>

#include <QApplication>
#include <QFontMetrics>
#include <QMouseEvent>

#include "ruler.hpp"
#include "view.hpp"

#include <pv/util.hpp>
#include <pv/widgets/popup.hpp>

using namespace Qt;

using std::shared_ptr;
using std::vector;

namespace pv {
namespace view {

const int Ruler::RulerHeight = 30;
const int Ruler::MinorTickSubdivision = 4;

const int Ruler::HoverArrowSize = 5;

const int Ruler::Padding = 20;
const int Ruler::BaselineOffset = 5;

Ruler::Ruler(View &parent) :
	MarginWidget(parent)
{
	setMouseTracking(true);

	connect(&view_, SIGNAL(hover_point_changed()),
		this, SLOT(hover_point_changed()));
}

void Ruler::clear_selection()
{
	const vector< shared_ptr<TimeItem> > items(view_.time_items());
	for (auto &i : items)
		i->select(false);
	update();
}

QSize Ruler::sizeHint() const
{
	return QSize(0, RulerHeight);
}

QSize Ruler::extended_size_hint() const
{
	const int text_height = calculate_text_height();
	return QSize(0, RulerHeight +
		(text_height + Padding + BaselineOffset) / 2);
}

void Ruler::paintEvent(QPaintEvent*)
{
	const int ValueMargin = 3;

	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);

	const double tick_period = view_.tick_period();
	const unsigned int prefix = view_.tick_prefix();

	// Draw the tick marks
	p.setPen(palette().color(foregroundRole()));

	const double minor_tick_period = tick_period / MinorTickSubdivision;
	const double first_major_division =
		floor(view_.offset() / tick_period);
	const double first_minor_division =
		ceil(view_.offset() / minor_tick_period);
	const double t0 = first_major_division * tick_period;

	int division = (int)round(first_minor_division -
		first_major_division * MinorTickSubdivision) - 1;

	const int text_height = calculate_text_height();
	const int major_tick_y1 = text_height + ValueMargin * 2;
	const int tick_y2 = RulerHeight;
	const int minor_tick_y1 = (major_tick_y1 + tick_y2) / 2;

	double x;

	do {
		const double t = t0 + division * minor_tick_period;
		x = (t - view_.offset()) / view_.scale();

		if (division % MinorTickSubdivision == 0)
		{
			// Draw a major tick
			p.drawText(x, ValueMargin, 0, text_height,
				AlignCenter | AlignTop | TextDontClip,
				pv::util::format_time(t, prefix));
			p.drawLine(QPointF(x, major_tick_y1),
				QPointF(x, tick_y2));
		}
		else
		{
			// Draw a minor tick
			p.drawLine(QPointF(x, minor_tick_y1),
				QPointF(x, tick_y2));
		}

		division++;

	} while (x < width());

	// Draw the hover mark
	draw_hover_mark(p);

	// The cursor labels are not drawn with the arrows exactly on the
	// bottom line of the widget, because then the selection shadow
	// would be clipped away.
	const QRect r = rect().adjusted(0, 0, 0, -BaselineOffset);

	// Draw the items
	const vector< shared_ptr<TimeItem> > items(view_.time_items());
	for (auto &i : items) {
		const bool highlight = !dragging_ &&
			i->label_rect(r).contains(mouse_point_);
		i->paint_label(p, r, highlight);
	}
}

void Ruler::mouseMoveEvent(QMouseEvent *e)
{
	mouse_point_ = e->pos();

	if (!(e->buttons() & Qt::LeftButton))
		return;

	if ((e->pos() - mouse_down_point_).manhattanLength() <
		QApplication::startDragDistance())
		return;

	// Do the drag
	dragging_ = true;

	const int delta = e->pos().x() - mouse_down_point_.x();
	const vector< shared_ptr<TimeItem> > items(view_.time_items());
	for (auto &i : items)
		if (i->dragging())
			i->set_time(view_.offset() +
				(i->drag_point().x() + delta - 0.5) *
				view_.scale());
}

void Ruler::mousePressEvent(QMouseEvent *e)
{
	if (e->buttons() & Qt::LeftButton) {
		mouse_down_point_ = e->pos();

		mouse_down_item_.reset();

		clear_selection();

		const vector< shared_ptr<TimeItem> > items(view_.time_items());
		for (auto i = items.rbegin(); i != items.rend(); i++)
			if ((*i)->label_rect(rect()).contains(e->pos())) {
				mouse_down_item_ = (*i);
				break;
			}

		if (mouse_down_item_) {
			mouse_down_item_->select();
			mouse_down_item_->drag();
		}

		selection_changed();
	}
}

void Ruler::mouseReleaseEvent(QMouseEvent *)
{
	using pv::widgets::Popup;

	if (!dragging_ && mouse_down_item_) {
		Popup *const p = mouse_down_item_->create_popup(&view_);
		if (p) {
			const QPoint arrpos(mouse_down_item_->get_x(),
				height() - BaselineOffset);
			p->set_position(mapToGlobal(arrpos), Popup::Bottom);
			p->show();
		}
	}

	dragging_ = false;
	mouse_down_item_.reset();

	const vector< shared_ptr<TimeItem> > items(view_.time_items());
	for (auto &i : items)
		i->drag_release();
}

void Ruler::leaveEvent(QEvent*)
{
	mouse_point_ = QPoint(-1, -1);
	update();
}

void Ruler::mouseDoubleClickEvent(QMouseEvent *e)
{
	view_.add_flag(view_.offset() + ((double)e->x() + 0.5) * view_.scale());
}

void Ruler::keyPressEvent(QKeyEvent *e)
{
	assert(e);

	if (e->key() == Qt::Key_Delete)
	{
		const vector< shared_ptr<TimeItem> > items(view_.time_items());
		for (auto &i : items)
			if (i->selected())
				i->delete_pressed();
	}
}

void Ruler::draw_hover_mark(QPainter &p)
{
	const int x = view_.hover_point().x();

	if (x == -1)
		return;

	p.setPen(QPen(Qt::NoPen));
	p.setBrush(QBrush(palette().color(foregroundRole())));

	const int b = RulerHeight;
	const QPointF points[] = {
		QPointF(x, b),
		QPointF(x - HoverArrowSize, b - HoverArrowSize),
		QPointF(x + HoverArrowSize, b - HoverArrowSize)
	};
	p.drawPolygon(points, countof(points));
}

int Ruler::calculate_text_height() const
{
	QFontMetrics fm(font());
	return fm.boundingRect(0, 0, INT_MAX, INT_MAX,
		Qt::AlignLeft | Qt::AlignTop, "8").height();
}

void Ruler::hover_point_changed()
{
	update();
}

} // namespace view
} // namespace pv
