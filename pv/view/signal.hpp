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
#include <QWidgetAction>

#include <stdint.h>

#include "signalscalehandle.hpp"
#include "trace.hpp"
#include "viewitemowner.hpp"

namespace pv {

class Session;

namespace data {
class SignalBase;
class SignalData;
}

namespace views {
namespace TraceView {

class Signal : public Trace, public ViewItemOwner
{
	Q_OBJECT

protected:
	Signal(pv::Session &session,
		std::shared_ptr<data::SignalBase> channel);

public:
	/**
	 * Sets the name of the signal.
	 */
	virtual void set_name(QString name);

	virtual std::shared_ptr<pv::data::SignalData> data() const = 0;

	/**
	 * Returns true if the trace is visible and enabled.
	 */
	bool enabled() const;

	std::shared_ptr<data::SignalBase> base() const;

	virtual void save_settings(QSettings &settings) const;

	virtual void restore_settings(QSettings &settings);

	/**
	 * Returns a list of row items owned by this object.
	 */
	const item_list& child_items() const;

	void paint_back(QPainter &p, const ViewItemPaintParams &pp);

	virtual void populate_popup_form(QWidget *parent, QFormLayout *form);

	QMenu* create_context_menu(QWidget *parent);

	void delete_pressed();

	/**
	 * Returns the offset to show the drag handle.
	 */
	virtual int scale_handle_offset() const = 0;

	/**
	 * Handles the scale handle being dragged to an offset.
	 * @param offset the offset the scale handle was dragged to.
	 */
	virtual void scale_handle_dragged(int offset) = 0;

	/**
	 * Handles the scale handle being being released.
	 */
	virtual void scale_handle_released() {};

protected Q_SLOTS:
	virtual void on_name_changed(const QString &text);

	void on_disable();

	void on_enabled_changed(bool enabled);

protected:
	pv::Session &session_;

	const std::shared_ptr<SignalScaleHandle> scale_handle_;
	const item_list items_;

	QComboBox *name_widget_;
};

} // namespace TraceView
} // namespace views
} // namespace pv

#endif // PULSEVIEW_PV_VIEWS_TRACEVIEW_SIGNAL_HPP
