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

#include "header.hpp"
#include "view.hpp"

#include "signal.hpp"
#include "tracegroup.hpp"

#include <algorithm>
#include <cassert>

#include <boost/iterator/filter_iterator.hpp>

#include <QApplication>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QRect>

#include <pv/session.hpp>
#include <pv/widgets/popup.hpp>

using boost::make_filter_iterator;

using std::count_if;
using std::dynamic_pointer_cast;
using std::shared_ptr;
using std::stable_sort;
using std::vector;

namespace pv {
namespace views {
namespace trace {

const int Header::Padding = 12;

static bool item_selected(shared_ptr<TraceTreeItem> r)
{
	return r->selected();
}

Header::Header(View &parent) :
	MarginWidget(parent)
{
}

QSize Header::sizeHint() const
{
	QRectF max_rect(-Padding, 0, Padding, 0);
	const vector<shared_ptr<TraceTreeItem>> items(
		view_.list_by_type<TraceTreeItem>());
	for (auto &i : items)
		if (i->enabled())
			max_rect = max_rect.united(i->label_rect(QRect()));
	return QSize(max_rect.width() + Padding, 0);
}

QSize Header::extended_size_hint() const
{
	return sizeHint() + QSize(ViewItem::HighlightRadius, 0);
}

vector< shared_ptr<ViewItem> > Header::items()
{
	const vector<shared_ptr<TraceTreeItem>> items(
		view_.list_by_type<TraceTreeItem>());
	return vector< shared_ptr<ViewItem> >(items.begin(), items.end());
}

shared_ptr<ViewItem> Header::get_mouse_over_item(const QPoint &pt)
{
	const QRect r(0, 0, width(), height());
	const vector<shared_ptr<TraceTreeItem>> items(
		view_.list_by_type<TraceTreeItem>());
	for (auto i = items.rbegin(); i != items.rend(); i++)
		if ((*i)->enabled() && (*i)->label_rect(r).contains(pt))
			return *i;
	return shared_ptr<TraceTreeItem>();
}

void Header::paintEvent(QPaintEvent*)
{
	const QRect rect(0, 0, width(), height());

	vector< shared_ptr<RowItem> > items(view_.list_by_type<RowItem>());

	stable_sort(items.begin(), items.end(),
		[](const shared_ptr<RowItem> &a, const shared_ptr<RowItem> &b) {
			return a->point(QRect()).y() < b->point(QRect()).y(); });

	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	for (const shared_ptr<RowItem> r : items) {
		assert(r);

		const bool highlight = !item_dragging_ &&
			r->label_rect(rect).contains(mouse_point_);
		r->paint_label(painter, rect, highlight);
	}

	painter.end();
}

void Header::contextMenuEvent(QContextMenuEvent *event)
{
	const shared_ptr<ViewItem> r = get_mouse_over_item(mouse_point_);
	if (!r)
		return;

	QMenu *menu = r->create_context_menu(this);
	if (!menu)
		menu = new QMenu(this);

	const vector< shared_ptr<TraceTreeItem> > items(
		view_.list_by_type<TraceTreeItem>());
	if (count_if(items.begin(), items.end(), item_selected) > 1) {
		menu->addSeparator();

		QAction *const group = new QAction(tr("Group"), this);
		QList<QKeySequence> shortcuts;
		shortcuts.append(QKeySequence(Qt::ControlModifier | Qt::Key_G));
		group->setShortcuts(shortcuts);
		connect(group, SIGNAL(triggered()), this, SLOT(on_group()));
		menu->addAction(group);
	}

	menu->exec(event->globalPos());
}

void Header::keyPressEvent(QKeyEvent *event)
{
	assert(event);

	MarginWidget::keyPressEvent(event);

	if (event->key() == Qt::Key_G && event->modifiers() == Qt::ControlModifier)
		on_group();
	else if (event->key() == Qt::Key_U && event->modifiers() == Qt::ControlModifier)
		on_ungroup();
}

void Header::on_group()
{
	const vector< shared_ptr<TraceTreeItem> > items(
		view_.list_by_type<TraceTreeItem>());
	vector< shared_ptr<TraceTreeItem> > selected_items(
		make_filter_iterator(item_selected, items.begin(), items.end()),
		make_filter_iterator(item_selected, items.end(), items.end()));
	stable_sort(selected_items.begin(), selected_items.end(),
		[](const shared_ptr<TraceTreeItem> &a, const shared_ptr<TraceTreeItem> &b) {
			return a->visual_v_offset() < b->visual_v_offset(); });

	shared_ptr<TraceGroup> group(new TraceGroup());
	shared_ptr<TraceTreeItem> mouse_down_item(
		dynamic_pointer_cast<TraceTreeItem>(mouse_down_item_));
	shared_ptr<TraceTreeItem> focus_item(
		mouse_down_item ? mouse_down_item : selected_items.front());

	assert(focus_item);
	assert(focus_item->owner());
	focus_item->owner()->add_child_item(group);

	// Set the group v_offset here before reparenting
	group->force_to_v_offset(focus_item->layout_v_offset() +
		focus_item->v_extents().first);

	for (size_t i = 0; i < selected_items.size(); i++) {
		const shared_ptr<TraceTreeItem> &r = selected_items[i];
		assert(r->owner());
		r->owner()->remove_child_item(r);
		group->add_child_item(r);

		// Put the items at 1-pixel offsets, so that restack will
		// stack them in the right order
		r->set_layout_v_offset(i);
	}
}

void Header::on_ungroup()
{
	bool restart;
	do {
		restart = false;
		const vector< shared_ptr<TraceGroup> > groups(
			view_.list_by_type<TraceGroup>());
		for (const shared_ptr<TraceGroup> tg : groups)
			if (tg->selected()) {
				tg->ungroup();
				restart = true;
				break;
			}
	} while (restart);
}

} // namespace trace
} // namespace views
} // namespace pv
