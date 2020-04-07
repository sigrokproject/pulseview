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

#include <QMainWindow>
#include <QTimer>
#include <QWidget>

#include <pv/data/signalbase.hpp>
#include <pv/util.hpp>

#ifdef ENABLE_DECODE
#include <pv/data/decodesignal.hpp>
#endif

using std::shared_ptr;
using std::vector;

namespace pv {

class Session;

namespace view {
class DecodeTrace;
class Signal;
}

namespace views {

// When adding an entry here, don't forget to update ViewTypeNames as well
enum ViewType {
	ViewTypeTrace,
#ifdef ENABLE_DECODE
	ViewTypeDecoderBinary,
	ViewTypeTabularDecoder,
#endif
	ViewTypeCount  // Indicates how many view types there are, must always be last
};

extern const char* ViewTypeNames[ViewTypeCount];

class ViewBase : public QWidget
{
	Q_OBJECT

public:
	static const int MaxViewAutoUpdateRate;

public:
	explicit ViewBase(Session &session, bool is_main_view = false, QMainWindow *parent = nullptr);

	virtual ViewType get_type() const = 0;
	bool is_main_view() const;

	/**
	 * Resets the view to its default state after construction. It does however
	 * not reset the signal bases or any other connections with the session.
	 */
	virtual void reset_view_state();

	Session& session();
	const Session& session() const;

	virtual void clear_signals();

	/**
	 * Returns the signal bases contained in this view.
	 */
	vector< shared_ptr<data::SignalBase> > signalbases() const;

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
	virtual void trigger_event(int segment_id, util::Timestamp location);
	virtual void signals_changed();
	virtual void capture_state_updated(int state);
	virtual void on_new_segment(int new_segment_id);
	virtual void on_segment_completed(int new_segment_id);
	virtual void perform_delayed_view_update();

private Q_SLOTS:
	void on_samples_added(uint64_t segment_id, uint64_t start_sample,
		uint64_t end_sample);

	void on_data_updated();

protected:
	Session &session_;

	const bool is_main_view_;

	util::TimeUnit time_unit_;

	vector< shared_ptr<data::SignalBase> > signalbases_;
#ifdef ENABLE_DECODE
	vector< shared_ptr<data::DecodeSignal> > decode_signals_;
#endif

	/// The ID of the currently displayed segment
	uint32_t current_segment_;

	QTimer delayed_view_updater_;
};

} // namespace views
} // namespace pv

#endif // PULSEVIEW_PV_VIEWS_VIEWBASE_HPP
