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

#include "decodergroupbox.h"

#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolBar>
#include <QVBoxLayout>

#include <assert.h>

namespace pv {
namespace widgets {

DecoderGroupBox::DecoderGroupBox(QString title, QWidget *parent) :
	QWidget(parent),
	_layout(new QGridLayout)
{
	_layout->setContentsMargins(0, 0, 0, 0);
	setLayout(_layout);

	_layout->addWidget(new QLabel(QString("<h3>%1</h3>").arg(title)),
		0, 0);
	_layout->setColumnStretch(0, 1);

	QToolBar *const toolbar = new QToolBar;
	toolbar->setIconSize(QSize(16, 16));
	_layout->addWidget(toolbar, 0, 1);

	QAction *const delete_action = new QAction(
		QIcon(":/icons/decoder-delete.svg"), tr("Delete"), this);
	connect(delete_action, SIGNAL(triggered()),
		this, SIGNAL(delete_decoder()));
	toolbar->addAction(delete_action);
}

void DecoderGroupBox::add_layout(QLayout *layout)
{
	assert(layout);
	_layout->addLayout(layout, 1, 0, 1, 2);
}

} // widgets
} // pv
