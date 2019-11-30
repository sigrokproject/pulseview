/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2019 Soeren Apel <soeren@apelpie.net>
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

#include <libsigrokdecode/libsigrokdecode.h>

#include <QLabel>
#include <QMenu>
#include <QToolBar>
#include <QVBoxLayout>

#include "view.hpp"
#include "QHexView.hpp"

#include "pv/session.hpp"
#include "pv/util.hpp"

using pv::data::SignalBase;
using pv::util::TimeUnit;
using pv::util::Timestamp;

using std::shared_ptr;

namespace pv {
namespace views {
namespace decoder_output {

View::View(Session &session, bool is_main_view, QMainWindow *parent) :
	ViewBase(session, is_main_view, parent),

	// Note: Place defaults in View::reset_view_state(), not here
	signal_selector_(new QComboBox()),
	format_selector_(new QComboBox()),
	stacked_widget_(new QStackedWidget()),
	hex_view_(new QHexView())
{
	QVBoxLayout *root_layout = new QVBoxLayout(this);
	root_layout->setContentsMargins(0, 0, 0, 0);

	// Create toolbar
	QToolBar* toolbar = new QToolBar();
	toolbar->setContextMenuPolicy(Qt::PreventContextMenu);
	parent->addToolBar(toolbar);

	// Populate toolbar
	toolbar->addWidget(new QLabel(tr("Decoder:")));
	toolbar->addWidget(signal_selector_);
	toolbar->addSeparator();
	toolbar->addWidget(new QLabel(tr("Show data as")));
	toolbar->addWidget(format_selector_);

	// Add format types
	format_selector_->addItem(tr("Hexdump"), qVariantFromValue(QString("text/hexdump")));

	// Add widget stack
	root_layout->addWidget(stacked_widget_);
	stacked_widget_->addWidget(hex_view_);

	reset_view_state();
}

View::~View()
{
}

ViewType View::get_type() const
{
	return ViewTypeDecoderOutput;
}

void View::reset_view_state()
{
	ViewBase::reset_view_state();
}

void View::clear_signals()
{
	ViewBase::clear_signalbases();
}

void View::clear_decode_signals()
{
	signal_selector_->clear();
	format_selector_->setCurrentIndex(0);
}

void View::add_decode_signal(shared_ptr<data::DecodeSignal> signal)
{
	connect(signal.get(), SIGNAL(name_changed(const QString&)),
		this, SLOT(on_signal_name_changed(const QString&)));

	signal_selector_->addItem(signal->name(), QVariant::fromValue(signal.get()));
}

void View::remove_decode_signal(shared_ptr<data::DecodeSignal> signal)
{
	int index = signal_selector_->findData(QVariant::fromValue(signal.get()));

	if (index != -1)
		signal_selector_->removeItem(index);
}

void View::save_settings(QSettings &settings) const
{
	(void)settings;
}

void View::restore_settings(QSettings &settings)
{
	// Note: It is assumed that this function is only called once,
	// immediately after restoring a previous session.
	(void)settings;
}

void View::on_signal_name_changed(const QString &name)
{
	SignalBase *sb = qobject_cast<SignalBase*>(QObject::sender());
	assert(sb);

	int index = signal_selector_->findData(QVariant::fromValue(sb));
	if (index != -1)
		signal_selector_->setItemText(index, name);
}

} // namespace decoder_output
} // namespace views
} // namespace pv
