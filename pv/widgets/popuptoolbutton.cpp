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

#include <assert.h>

#include "popuptoolbutton.hpp"

namespace pv {
namespace widgets {

PopupToolButton::PopupToolButton(QWidget *parent) :
	QToolButton(parent),
	popup_(nullptr)
{
	connect(this, SIGNAL(clicked(bool)), this, SLOT(on_clicked(bool)));
}

Popup* PopupToolButton::popup() const
{
	return popup_;
}

void PopupToolButton::set_popup(Popup *popup)
{
	assert(popup);
	popup_ = popup;
}

void PopupToolButton::on_clicked(bool)
{
	if (!popup_)
		return;

	const QRect r = rect();
	popup_->set_position(mapToGlobal(QPoint((r.left() + r.right()) / 2,
		((r.top() + r.bottom() * 3) / 4))), Popup::Bottom);
	popup_->show();
}

} // widgets
} // pv
