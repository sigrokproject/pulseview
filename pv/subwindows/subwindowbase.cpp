/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2018 Soeren Apel <soeren@apelpie.net>
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

#ifdef ENABLE_DECODE
#include <libsigrokdecode/libsigrokdecode.h>
#endif

#include <QWidget>

#include "pv/session.hpp"
#include "pv/subwindows/subwindowbase.hpp"

using std::shared_ptr;

namespace pv {
namespace subwindows {

SubWindowBase::SubWindowBase(Session &session, QWidget *parent) :
	QWidget(parent),
	session_(session)
{
	connect(&session_, SIGNAL(signals_changed()), this, SLOT(on_signals_changed()));
}

bool SubWindowBase::has_toolbar() const
{
	return false;
}

QToolBar* SubWindowBase::create_toolbar(QWidget *parent) const
{
	(void)parent;

	return nullptr;
}

Session& SubWindowBase::session()
{
	return session_;
}

const Session& SubWindowBase::session() const
{
	return session_;
}

unordered_set< shared_ptr<data::SignalBase> > SubWindowBase::signalbases() const
{
	return signalbases_;
}

void SubWindowBase::clear_signalbases()
{
	for (shared_ptr<data::SignalBase> signalbase : signalbases_) {
		disconnect(signalbase.get(), SIGNAL(samples_cleared()),
			this, SLOT(on_data_updated()));
		disconnect(signalbase.get(), SIGNAL(samples_added(uint64_t, uint64_t, uint64_t)),
			this, SLOT(on_samples_added(uint64_t, uint64_t, uint64_t)));
	}

	signalbases_.clear();
}

void SubWindowBase::add_signalbase(const shared_ptr<data::SignalBase> signalbase)
{
	signalbases_.insert(signalbase);
}

#ifdef ENABLE_DECODE
void SubWindowBase::clear_decode_signals()
{
}

void SubWindowBase::add_decode_signal(shared_ptr<data::DecodeSignal> signal)
{
	(void)signal;
}

void SubWindowBase::remove_decode_signal(shared_ptr<data::DecodeSignal> signal)
{
	(void)signal;
}
#endif

void SubWindowBase::on_signals_changed()
{
}

} // namespace subwindows
} // namespace pv
