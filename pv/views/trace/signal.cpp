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

#include <extdef.h>

#include <cassert>
#include <cmath>

#include <QApplication>
#include <QFormLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>

#include <libsigrokcxx/libsigrokcxx.hpp>

#include "pv/data/signalbase.hpp"

#include "signal.hpp"
#include "view.hpp"

using std::shared_ptr;
using std::make_shared;

namespace pv {
namespace views {
namespace trace {

const char *const ChannelNames[] = {
	"CLK",
	"DATA",
	"IN",
	"OUT",
	"RST",
	"TX",
	"RX",
	"EN",
	"SCLK",
	"MOSI",
	"MISO",
	"/SS",
	"SDA",
	"SCL"
};

Signal::Signal(pv::Session &session,
	shared_ptr<data::SignalBase> channel) :
	Trace(channel),
	session_(session),
	name_widget_(nullptr)
{
	assert(base_);

	connect(base_.get(), SIGNAL(enabled_changed(bool)),
		this, SLOT(on_enabled_changed(bool)));
}

void Signal::set_name(QString name)
{
	Trace::set_name(name);

	if (name != name_widget_->currentText())
		name_widget_->setEditText(name);
}

bool Signal::enabled() const
{
	return base_->enabled();
}

shared_ptr<data::SignalBase> Signal::base() const
{
	return base_;
}

void Signal::save_settings(QSettings &settings) const
{
	(void)settings;
}

void Signal::restore_settings(QSettings &settings)
{
	(void)settings;
}

void Signal::paint_back(QPainter &p, ViewItemPaintParams &pp)
{
	if (base_->enabled())
		Trace::paint_back(p, pp);
}

void Signal::populate_popup_form(QWidget *parent, QFormLayout *form)
{
	name_widget_ = new QComboBox(parent);
	name_widget_->setEditable(true);
	name_widget_->setCompleter(nullptr);

	for (unsigned int i = 0; i < countof(ChannelNames); i++)
		name_widget_->insertItem(i, ChannelNames[i]);

	const int index = name_widget_->findText(base_->name(), Qt::MatchExactly);

	if (index == -1) {
		name_widget_->insertItem(0, base_->name());
		name_widget_->setCurrentIndex(0);
	} else {
		name_widget_->setCurrentIndex(index);
	}

	connect(name_widget_, SIGNAL(editTextChanged(const QString&)),
		this, SLOT(on_nameedit_changed(const QString&)));

	form->addRow(tr("Name"), name_widget_);

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

void Signal::on_name_changed(const QString &text)
{
	// On startup, this event is fired when a session restores signal
	// names. However, the name widget hasn't yet been created.
	if (!name_widget_)
		return;

	if (text != name_widget_->currentText())
		name_widget_->setEditText(text);

	Trace::on_name_changed(text);
}

void Signal::on_disable()
{
	base_->set_enabled(false);
}

void Signal::on_enabled_changed(bool enabled)
{
	(void)enabled;

	if (owner_)
		owner_->extents_changed(true, true);
}

} // namespace trace
} // namespace views
} // namespace pv
