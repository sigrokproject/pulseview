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

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

#include "signal.hpp"
#include "view.hpp"
#include "viewitempaintparams.hpp"
#include "viewport.hpp"

#include <pv/session.hpp>

#include <QMouseEvent>
#include <QScreen>
#include <QWindow>

#include <QDebug>

using std::abs;
using std::back_inserter;
using std::copy;
using std::dynamic_pointer_cast;
using std::none_of; // NOLINT. Used in assert()s.
using std::shared_ptr;
using std::stable_sort;
using std::vector;

namespace pv {
namespace views {
namespace trace {

Viewport::Viewport(View &parent) :
	ViewWidget(parent),
	pinch_zoom_active_(false)
{
	setAutoFillBackground(true);
	setBackgroundRole(QPalette::Base);

	// Set up settings and event handlers
	GlobalSettings settings;
	allow_vertical_dragging_ = settings.value(GlobalSettings::Key_View_AllowVerticalDragging).toBool();

	GlobalSettings::add_change_handler(this);
}

Viewport::~Viewport()
{
	GlobalSettings::remove_change_handler(this);
}

shared_ptr<ViewItem> Viewport::get_mouse_over_item(const QPoint &pt)
{
	const ViewItemPaintParams pp(rect(), view_.scale(), view_.offset());
	const vector< shared_ptr<ViewItem> > items(this->items());
	for (auto i = items.rbegin(); i != items.rend(); i++)
		if ((*i)->enabled() && (*i)->hit_box_rect(pp).contains(pt))
			return *i;
	return nullptr;
}

void Viewport::item_hover(const shared_ptr<ViewItem> &item, QPoint pos)
{
	if (item && item->is_draggable(pos))
		setCursor(dynamic_pointer_cast<ViewItem>(item) ?
			Qt::SizeHorCursor : Qt::SizeVerCursor);
	else
		unsetCursor();
}

void Viewport::drag()
{
	drag_offset_ = view_.offset();

	if (allow_vertical_dragging_)
		drag_v_offset_ = view_.owner_visual_v_offset();
}

void Viewport::drag_by(const QPoint &delta)
{
	if (drag_offset_ == boost::none)
		return;

	view_.set_scale_offset(view_.scale(),
		(*drag_offset_ - delta.x() * view_.scale()));

	if (allow_vertical_dragging_)
		view_.set_v_offset(-drag_v_offset_ - delta.y());
}

void Viewport::drag_release()
{
	drag_offset_ = boost::none;
}

vector< shared_ptr<ViewItem> > Viewport::items()
{
	vector< shared_ptr<ViewItem> > items;
	const vector< shared_ptr<ViewItem> > view_items(
		view_.list_by_type<ViewItem>());
	copy(view_items.begin(), view_items.end(), back_inserter(items));
	const vector< shared_ptr<TimeItem> > time_items(view_.time_items());
	copy(time_items.begin(), time_items.end(), back_inserter(items));
	return items;
}

bool Viewport::touch_event(QTouchEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	QList<QEventPoint> touchPoints = event->points();
#else
	QList<QTouchEvent::TouchPoint> touchPoints = event->touchPoints();
#endif

	if (touchPoints.count() != 2) {
		pinch_zoom_active_ = false;
		return false;
	}
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	if (event->device()->type() == QInputDevice::DeviceType::TouchPad) {
		return false;
	}

	const QEventPoint &touchPoint0 = touchPoints.first();
	const QEventPoint &touchPoint1 = touchPoints.last();

	if (!pinch_zoom_active_ ||
		(event->touchPointStates() & QEventPoint::Pressed)) {
		pinch_offset0_ = (view_.offset() + view_.scale() * touchPoint0.position().x()).convert_to<double>();
		pinch_offset1_ = (view_.offset() + view_.scale() * touchPoint1.position().x()).convert_to<double>();
		pinch_zoom_active_ = true;
	}

	double w = touchPoint1.position().x() - touchPoint0.position().x();
	if (abs(w) >= 1.0) {
		const double scale =
			fabs((pinch_offset1_ - pinch_offset0_) / w);
		double offset = pinch_offset0_ - touchPoint0.position().x() * scale;
		if (scale > 0)
			view_.set_scale_offset(scale, offset);
	}
#else
	if (event->device()->type() == QTouchDevice::TouchPad) {
		return false;
	}

	const QTouchEvent::TouchPoint &touchPoint0 = touchPoints.first();
	const QTouchEvent::TouchPoint &touchPoint1 = touchPoints.last();

	if (!pinch_zoom_active_ ||
	    (event->touchPointStates() & Qt::TouchPointPressed)) {
		pinch_offset0_ = (view_.offset() + view_.scale() * touchPoint0.pos().x()).convert_to<double>();
		pinch_offset1_ = (view_.offset() + view_.scale() * touchPoint1.pos().x()).convert_to<double>();
		pinch_zoom_active_ = true;
	}

	double w = touchPoint1.pos().x() - touchPoint0.pos().x();
	if (abs(w) >= 1.0) {
		const double scale =
			fabs((pinch_offset1_ - pinch_offset0_) / w);
		double offset = pinch_offset0_ - touchPoint0.pos().x() * scale;
		if (scale > 0)
			view_.set_scale_offset(scale, offset);
	}
#endif

	if (event->touchPointStates() & Qt::TouchPointReleased) {
		pinch_zoom_active_ = false;

		if (touchPoint0.state() & Qt::TouchPointReleased) {
			// Primary touch released
			drag_release();
		} else {
			// Update the mouse down fields so that continued
			// dragging with the primary touch will work correctly
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
			mouse_down_point_ = touchPoint0.position().toPoint();
#else
			mouse_down_point_ = touchPoint0.pos().toPoint();
#endif
			drag();
		}
	}

	return true;
}

void Viewport::paintEvent(QPaintEvent*)
{
	typedef void (ViewItem::*LayerPaintFunc)(
		QPainter &p, ViewItemPaintParams &pp);
	LayerPaintFunc layer_paint_funcs[] = {
		&ViewItem::paint_back, &ViewItem::paint_mid,
		&ViewItem::paint_fore, nullptr};

	vector< shared_ptr<ViewItem> > row_items(view_.list_by_type<ViewItem>());
	assert(none_of(row_items.begin(), row_items.end(),
		[](const shared_ptr<ViewItem> &r) { return !r; }));

	stable_sort(row_items.begin(), row_items.end(),
		[](const shared_ptr<ViewItem> &a, const shared_ptr<ViewItem> &b) {
			return a->drag_point(QRect()).y() < b->drag_point(QRect()).y(); });

	const vector< shared_ptr<TimeItem> > time_items(view_.time_items());
	assert(none_of(time_items.begin(), time_items.end(),
		[](const shared_ptr<TimeItem> &t) { return !t; }));

	QPainter p(this);

	// Disable antialiasing for high-DPI displays
	bool use_antialiasing =
		window()->windowHandle()->screen()->devicePixelRatio() < 2.0;
	p.setRenderHint(QPainter::Antialiasing, use_antialiasing);

	for (LayerPaintFunc *paint_func = layer_paint_funcs;
			*paint_func; paint_func++) {
		ViewItemPaintParams time_pp(rect(), view_.scale(), view_.offset());
		for (const shared_ptr<TimeItem>& t : time_items)
			(t.get()->*(*paint_func))(p, time_pp);

		ViewItemPaintParams row_pp(rect(), view_.scale(), view_.offset());
		for (const shared_ptr<ViewItem>& r : row_items)
			(r.get()->*(*paint_func))(p, row_pp);
	}

	p.end();
}

void Viewport::mouseDoubleClickEvent(QMouseEvent *event)
{
	assert(event);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	if (event->buttons() & Qt::LeftButton)
		view_.zoom(2.0, event->position().x());
	else if (event->buttons() & Qt::RightButton)
		view_.zoom(-2.0, event->position().x());
#else
	if (event->buttons() & Qt::LeftButton)
		view_.zoom(2.0, event->x());
	else if (event->buttons() & Qt::RightButton)
		view_.zoom(-2.0, event->x());
#endif
}

void Viewport::wheelEvent(QWheelEvent *event)
{
	assert(event);

#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
	const QPoint delta = event->angleDelta();
	const int posX = event->position().x();
#else
	const int dx = event->orientation() == Qt::Horizontal ? event->delta() : 0;
	const int dy = event->orientation() == Qt::Vertical ? event->delta() : 0;

	const QPoint delta = QPoint(dx, dy);
	const int posX = event->x();
#endif

	// Trackpad can generate large deltas, bound x/y to prevent overdrive
	const int max_delta = 120;
	const int deltaX = qBound(-max_delta, delta.x(), max_delta);
	const int deltaY = qBound(-max_delta, delta.y(), max_delta);

	// Horizontal scrolling is interpreted as moving left/right
	if(deltaX != 0) {
		// For trackpad natural scrolling in horizontal plane is opposite
		// to mouse, since trackpad does not need Shift to scroll left/right,
		// use Shift to distinguish between trackpad and mouse
		view_.set_scale_offset(view_.scale(), 
			(event->modifiers() & Qt::ShiftModifier ? deltaX : -deltaX)
			* view_.scale() + view_.offset());
	}

	if(deltaY != 0) {
		if (event->modifiers() & Qt::ControlModifier) {
			// Vertical scrolling with the control key pressed
			// is intrepretted as vertical scrolling
			view_.set_v_offset(
				-view_.owner_visual_v_offset() - (deltaY * height()) / (8 * 120));
		} else {
			// Vertical scrolling is interpreted as zooming in/out
			view_.zoom(deltaY / 120.0, posX);
		}
	}
}

void Viewport::on_setting_changed(const QString &key, const QVariant &value)
{
	if (key == GlobalSettings::Key_View_AllowVerticalDragging)
		allow_vertical_dragging_ = value.toBool();
}

} // namespace trace
} // namespace views
} // namespace pv
