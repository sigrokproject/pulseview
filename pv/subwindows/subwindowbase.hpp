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

#ifndef PULSEVIEW_PV_SUBWINDOWBASE_HPP
#define PULSEVIEW_PV_SUBWINDOWBASE_HPP

#include <cstdint>
#include <memory>
#include <unordered_set>

#include <QToolBar>
#include <QWidget>

#include <pv/data/signalbase.hpp>

#ifdef ENABLE_DECODE
#include <pv/data/decodesignal.hpp>
#endif

using std::shared_ptr;
using std::unordered_set;

namespace pv {

class Session;

namespace subwindows {

enum SubWindowType {
	SubWindowTypeDecoderSelector,
};

class SubWindowBase : public QWidget
{
	Q_OBJECT

public:
	explicit SubWindowBase(Session &session, QWidget *parent = nullptr);

	virtual bool has_toolbar() const;
	virtual QToolBar* create_toolbar(QWidget *parent) const;

	Session& session();
	const Session& session() const;

	/**
	 * Returns the signal bases contained in this view.
	 */
	unordered_set< shared_ptr<data::SignalBase> > signalbases() const;

	virtual void clear_signalbases();

	virtual void add_signalbase(const shared_ptr<data::SignalBase> signalbase);

#ifdef ENABLE_DECODE
	virtual void clear_decode_signals();

	virtual void add_decode_signal(shared_ptr<data::DecodeSignal> signal);

	virtual void remove_decode_signal(shared_ptr<data::DecodeSignal> signal);
#endif

public Q_SLOTS:
	virtual void on_signals_changed();

protected:
	Session &session_;

	unordered_set< shared_ptr<data::SignalBase> > signalbases_;
};

} // namespace subwindows
} // namespace pv

#endif // PULSEVIEW_PV_SUBWINDOWBASE_HPP
