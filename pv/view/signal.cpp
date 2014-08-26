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

#include <assert.h>
#include <math.h>

#include <QApplication>
#include <QFormLayout>
#include <QLineEdit>
#include <QMenu>

#include <libsigrok/libsigrok.h>

#include "signal.h"
#include "view.h"

#include <pv/device/devinst.h>

using std::shared_ptr;

namespace pv {
namespace view {

const char *const ProbeNames[] = {
	"CLK",
	"DATA",
	"IN",
	"OUT",
	"RST",
	"Tx",
	"Rx",
	"EN",
	"SCLK",
	"MOSI",
	"MISO",
	"/SS",
	"SDA",
	"SCL"
};

Signal::Signal(shared_ptr<pv::device::DevInst> dev_inst,
	const sr_channel *const probe) :
	Trace(probe->name),
	_dev_inst(dev_inst),
	_probe(probe),
	_name_widget(NULL),
	_updating_name_widget(false)
{
	assert(_probe);
}

void Signal::set_name(QString name)
{
	Trace::set_name(name);
	_updating_name_widget = true;
	_name_widget->setEditText(name);
	_updating_name_widget = false;
}

bool Signal::enabled() const
{
	return _probe->enabled;
}

void Signal::enable(bool enable)
{
	_dev_inst->enable_probe(_probe, enable);
	visibility_changed();
}

const sr_channel* Signal::probe() const
{
	return _probe;
}

void Signal::populate_popup_form(QWidget *parent, QFormLayout *form)
{
	_name_widget = new QComboBox(parent);
	_name_widget->setEditable(true);

	for(unsigned int i = 0; i < countof(ProbeNames); i++)
		_name_widget->insertItem(i, ProbeNames[i]);
	_name_widget->setEditText(_name);
	_name_widget->lineEdit()->selectAll();
	_name_widget->setFocus();

	connect(_name_widget, SIGNAL(editTextChanged(const QString&)),
		this, SLOT(on_text_changed(const QString&)));

	form->addRow(tr("Name"), _name_widget);

	add_colour_option(parent, form);
}

QMenu* Signal::create_context_menu(QWidget *parent)
{
	QMenu *const menu = Trace::create_context_menu(parent);

	menu->addSeparator();

	QAction *const disable = new QAction(tr("Disable"), this);
	disable->setShortcuts(QKeySequence::Delete);
	connect(disable, SIGNAL(triggered()), this, SLOT(on_disable()));
	menu->addAction(disable);

	return menu;
}

void Signal::delete_pressed()
{
	on_disable();
}

void Signal::on_disable()
{
	enable(false);
}

} // namespace view
} // namespace pv
