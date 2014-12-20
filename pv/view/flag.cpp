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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "timemarker.hpp"
#include "view.hpp"

#include <QColor>
#include <QFormLayout>
#include <QLineEdit>

#include <libsigrok/libsigrok.hpp>

#include <pv/widgets/popup.hpp>

using std::shared_ptr;

namespace pv {
namespace view {

const QColor Flag::FillColour(0x73, 0xD2, 0x16);

Flag::Flag(View &view, double time, const QString &text) :
	TimeMarker(view, FillColour, time),
	text_(text)
{
}

Flag::Flag(const Flag &flag) :
	TimeMarker(flag.view_, FillColour, flag.time_),
	std::enable_shared_from_this<pv::view::Flag>(flag)
{
}

bool Flag::enabled() const
{
	return true;
}

QString Flag::get_text() const
{
	return text_;
}

pv::widgets::Popup* Flag::create_popup(QWidget *parent)
{
	pv::widgets::Popup *const popup = TimeMarker::create_popup(parent);
	QFormLayout *const form = (QFormLayout*)popup->layout();

	QLineEdit *const text_edit = new QLineEdit(popup);
	text_edit->setText(text_);

	connect(text_edit, SIGNAL(textChanged(const QString&)),
		this, SLOT(on_text_changed(const QString&)));

	form->insertRow(0, tr("Text"), text_edit);

	return popup;
}

void Flag::delete_pressed()
{
	view_.remove_flag(shared_ptr<Flag>(shared_from_this()));
}

void Flag::on_text_changed(const QString &text)
{
	text_ = text;
	view_.time_item_appearance_changed(true, false);
}

} // namespace view
} // namespace pv
