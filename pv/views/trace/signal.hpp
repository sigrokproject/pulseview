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

#ifndef PULSEVIEW_PV_VIEWS_TRACEVIEW_SIGNAL_HPP
#define PULSEVIEW_PV_VIEWS_TRACEVIEW_SIGNAL_HPP

#include <memory>

#include <QComboBox>
#include <QString>
#include <QVariant>
#include <QWidgetAction>

#include <cstdint>

#include <pv/data/logicsegment.hpp>

#include "trace.hpp"
#include "viewitemowner.hpp"

using std::shared_ptr;

namespace pv {

class Session;

namespace data {
class SignalBase;
class SignalData;
}

namespace views {
namespace trace {

/**
 * The Signal class represents a series of numeric values that can be drawn.
 * This is the main difference to the more generic @ref Trace class.
 *
 * It is generally accepted that Signal instances consider themselves to be
 * individual channels on e.g. an oscilloscope, though it should be kept in
 * mind that virtual signals (e.g. math) will also be served by the Signal
 * class.
 */
class Signal : public Trace, public ViewItemOwner
{
	Q_OBJECT

protected:
	Signal(pv::Session &session, shared_ptr<data::SignalBase> channel);

public:
	/**
	 * Sets the name of the signal.
	 */
	virtual void set_name(QString name);

	virtual shared_ptr<pv::data::SignalData> data() const = 0;

	/**
	 * Determines the closest level change (i.e. edge) to a given sample, which
	 * is useful for e.g. the "snap to edge" functionality.
	 *
	 * @param sample_pos Sample to use
	 * @return The changes left and right of the given position
	 */
	virtual vector<data::LogicSegment::EdgePair> get_nearest_level_changes(uint64_t sample_pos) = 0;

	/**
	 * Returns true if the trace is visible and enabled.
	 */
	bool enabled() const;

	shared_ptr<data::SignalBase> base() const;

	virtual void save_settings(QSettings &settings) const;
	virtual std::map<QString, QVariant> save_settings() const;

	virtual void restore_settings(QSettings &settings);
	virtual void restore_settings(std::map<QString, QVariant> settings);

	void paint_back(QPainter &p, ViewItemPaintParams &pp);

	virtual void populate_popup_form(QWidget *parent, QFormLayout *form);

	QMenu* create_header_context_menu(QWidget *parent);

	void delete_pressed();

protected Q_SLOTS:
	virtual void on_name_changed(const QString &text);

	void on_disable();

	void on_enabled_changed(bool enabled);

protected:
	pv::Session &session_;

	QComboBox *name_widget_;
};

} // namespace trace
} // namespace views
} // namespace pv

#endif // PULSEVIEW_PV_VIEWS_TRACEVIEW_SIGNAL_HPP
