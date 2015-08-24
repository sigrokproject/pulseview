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

#include "decodergroupbox.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <assert.h>

namespace pv {
namespace widgets {

DecoderGroupBox::DecoderGroupBox(QString title, QWidget *parent, bool isDeletable) :
	QWidget(parent),
	layout_(new QGridLayout),
	show_hide_button_(QIcon(":/icons/decoder-shown.svg"), QString(), this)
{
	layout_->setContentsMargins(0, 0, 0, 0);
	setLayout(layout_);

	layout_->addWidget(new QLabel(QString("<h3>%1</h3>").arg(title)),
		0, 0);
	layout_->setColumnStretch(0, 1);

	QHBoxLayout *const toolbar = new QHBoxLayout;
	layout_->addLayout(toolbar, 0, 1);

	show_hide_button_.setFlat(true);
	show_hide_button_.setIconSize(QSize(16, 16));
	connect(&show_hide_button_, SIGNAL(clicked()),
		this, SIGNAL(show_hide_decoder()));
	toolbar->addWidget(&show_hide_button_);

	if (isDeletable) {
		QPushButton *const delete_button = new QPushButton(
			QIcon(":/icons/decoder-delete.svg"), QString(), this);
		delete_button->setFlat(true);
		delete_button->setIconSize(QSize(16, 16));
		connect(delete_button, SIGNAL(clicked()),
			this, SIGNAL(delete_decoder()));
		toolbar->addWidget(delete_button);
	}
}

void DecoderGroupBox::add_layout(QLayout *layout)
{
	assert(layout);
	layout_->addLayout(layout, 1, 0, 1, 2);
}

void DecoderGroupBox::set_decoder_visible(bool visible)
{
	show_hide_button_.setIcon(QIcon(visible ?
		":/icons/decoder-shown.svg" :
		":/icons/decoder-hidden.svg"));
}

} // widgets
} // pv
