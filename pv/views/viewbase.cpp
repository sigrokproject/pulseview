/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2016 Soeren Apel <soeren@apelpie.net>
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

#include <libsigrokcxx/libsigrokcxx.hpp>

#include "pv/session.hpp"
#include "pv/util.hpp"

using std::shared_ptr;

namespace pv {
namespace views {

ViewBase::ViewBase(Session &session, bool is_main_view, QWidget *parent) :
	session_(session),
	is_main_view_(is_main_view)
{
	(void)parent;

	connect(&session_, SIGNAL(signals_changed()),
		this, SLOT(signals_changed()));
	connect(&session_, SIGNAL(capture_state_changed(int)),
		this, SLOT(capture_state_updated(int)));
	connect(&session_, SIGNAL(data_received()),
		this, SLOT(data_updated()));
	connect(&session_, SIGNAL(frame_ended()),
		this, SLOT(data_updated()));
}

Session& ViewBase::session()
{
	return session_;
}

const Session& ViewBase::session() const
{
	return session_;
}

void ViewBase::clear_signals()
{
}

#ifdef ENABLE_DECODE
void ViewBase::clear_decode_signals()
{
}

void ViewBase::add_decode_signal(shared_ptr<data::SignalBase> signalbase)
{
	(void)signalbase;
}

void ViewBase::remove_decode_signal(shared_ptr<data::SignalBase> signalbase)
{
	(void)signalbase;
}
#endif

void ViewBase::save_settings(QSettings &settings) const
{
	(void)settings;
}

void ViewBase::restore_settings(QSettings &settings)
{
	(void)settings;
}

void ViewBase::trigger_event(util::Timestamp location)
{
	(void)location;
}

void ViewBase::signals_changed()
{
}

void ViewBase::capture_state_updated(int state)
{
	(void)state;
}

void ViewBase::data_updated()
{
}

} // namespace view
} // namespace pv
