/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2014 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#include "timemarker.hpp"
#include "view.hpp"
#include "ruler.hpp"

#include <QColor>
#include <QFormLayout>
#include <QLineEdit>
#include <QMenu>
#include <QApplication>

#include <libsigrokcxx/libsigrokcxx.hpp>

#include <pv/widgets/popup.hpp>

using std::enable_shared_from_this;
using std::shared_ptr;

namespace pv {
namespace views {
namespace trace {

const QColor Flag::FillColor(0x73, 0xD2, 0x16);

Flag::Flag(View &view, const pv::util::Timestamp& time, const QString &text) :
	TimeMarker(view, FillColor, time),
	text_(text)
{
}

Flag::Flag(const Flag &flag) :
	TimeMarker(flag.view_, FillColor, flag.time_),
	enable_shared_from_this<Flag>(flag)
{
}

bool Flag::enabled() const
{
	return true;
}

/**
 * Returns the text used to display this flag item. This is not necessarily the
 * name that the user has given it.
 */
QString Flag::get_display_text() const
{
	QString s;

	const shared_ptr<TimeItem> ref_item = view_.ruler()->get_reference_item();

	if (!ref_item || (ref_item.get() == this))
		s = text_;
	else {
		const QString time = Ruler::format_time_with_distance(
			ref_item->time(), ref_item->delta(time_),
			view_.tick_prefix(), view_.time_unit(), view_.tick_precision());

		if(view_.time_unit() == pv::util::TimeUnit::Time) {
		    const QString freq = util::format_value_si(1/abs(ref_item->delta(time_).convert_to<double>()),
			  pv::util::SIPrefix::unspecified,
			   12, "Hz", false);
		    s = QString("%1 (%2)").arg(time, freq);
		}
		else
		    s = time;
	 }

	return s;
}

/**
 * Returns the text of this flag item, i.e. the user-editable name.
 */
QString Flag::get_text() const
{
	return text_;
}

void Flag::set_text(const QString &text)
{
	text_ = text;
	view_.time_item_appearance_changed(true, false);
}

QRectF Flag::label_rect(const QRectF &rect) const
{
	QRectF r;

	const shared_ptr<TimeItem> ref_item = view_.ruler()->get_reference_item();

	if (!ref_item || (ref_item.get() == this)) {
		r = TimeMarker::label_rect(rect);
	} else {
		// TODO: Remove code duplication between here and cursor.cpp
		const float x = get_x();

		QFontMetrics m(QApplication::font());
		QSize text_size = m.boundingRect(get_display_text()).size();

		const QSizeF label_size(
			text_size.width() + LabelPadding.width() * 2,
			text_size.height() + LabelPadding.height() * 2);

		const float height = label_size.height();
		const float top =
			rect.height() - label_size.height() - TimeMarker::ArrowSize - 0.5f;

		const pv::util::Timestamp& delta = ref_item->delta(time_);

		if (delta >= 0)
			r = QRectF(x, top, label_size.width(), height);
		else
			r = QRectF(x - label_size.width(), top, label_size.width(), height);
	}

	return r;
}

pv::widgets::Popup* Flag::create_popup(QWidget *parent)
{
	using pv::widgets::Popup;

	Popup *const popup = TimeMarker::create_popup(parent);
	popup->set_position(parent->mapToGlobal(
		drag_point(parent->rect())), Popup::Bottom);

	QFormLayout *const form = (QFormLayout*)popup->layout();

	QLineEdit *const text_edit = new QLineEdit(popup);
	text_edit->setText(text_);

	connect(text_edit, SIGNAL(textChanged(const QString&)),
		this, SLOT(on_text_changed(const QString&)));

	form->insertRow(0, tr("Text"), text_edit);

	return popup;
}

QMenu* Flag::create_header_context_menu(QWidget *parent)
{
	QMenu *const menu = new QMenu(parent);

	QAction *const del = new QAction(tr("Delete"), this);
	del->setShortcuts(QKeySequence::Delete);
	connect(del, SIGNAL(triggered()), this, SLOT(on_delete()));
	menu->addAction(del);

	QAction *const snap_disable = new QAction(tr("Disable snapping"), this);
	snap_disable->setCheckable(true);
	snap_disable->setChecked(snapping_disabled_);
	connect(snap_disable, &QAction::toggled, this, [=](bool checked){snapping_disabled_ = checked;});
	menu->addAction(snap_disable);

	return menu;
}

void Flag::delete_pressed()
{
	on_delete();
}

void Flag::on_delete()
{
	view_.remove_flag(shared_ptr<Flag>(shared_from_this()));
}

void Flag::on_text_changed(const QString &text)
{
	set_text(text);
}

} // namespace trace
} // namespace views
} // namespace pv
