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

#ifndef PULSEVIEW_PV_VIEWS_VIEWBASE_HPP
#define PULSEVIEW_PV_VIEWS_VIEWBASE_HPP

#include <cstdint>
#include <memory>
#include <unordered_set>
#include <vector>

#include <QTimer>
#include <QWidget>

#include <pv/data/signalbase.hpp>
#include <pv/util.hpp>

#ifdef ENABLE_DECODE
#include <pv/data/decodesignal.hpp>
#endif

using std::shared_ptr;
using std::unordered_set;

namespace pv {

class Session;

namespace view {
class DecodeTrace;
class Signal;
}

namespace views {

enum ViewType {
	ViewTypeTrace,
	ViewTypeTabularDecode
};

class ViewBase : public QWidget
{
	Q_OBJECT

private:
	static const int MaxViewAutoUpdateRate;

public:
	explicit ViewBase(Session &session, bool is_main_view = false, QWidget *parent = nullptr);

	Session& session();
	const Session& session() const;

	virtual void clear_signals();

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

	virtual void save_settings(QSettings &settings) const;

	virtual void restore_settings(QSettings &settings);

public Q_SLOTS:
	virtual void trigger_event(util::Timestamp location);
	virtual void signals_changed();
	virtual void capture_state_updated(int state);
	virtual void on_new_segment(int new_segment_id);
	virtual void on_segment_completed(int new_segment_id);
	virtual void perform_delayed_view_update();

private Q_SLOTS:
	void on_data_updated();

protected:
	Session &session_;

	const bool is_main_view_;

	util::TimeUnit time_unit_;

	unordered_set< shared_ptr<data::SignalBase> > signalbases_;

	QTimer delayed_view_updater_;
};

} // namespace views
} // namespace pv

#endif // PULSEVIEW_PV_VIEWS_VIEWBASE_HPP
