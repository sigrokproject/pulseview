/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2014 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#ifndef PULSEVIEW_PV_VIEWS_TRACEVIEW_FLAG_HPP
#define PULSEVIEW_PV_VIEWS_TRACEVIEW_FLAG_HPP

#include <memory>

#include "timemarker.hpp"

using std::enable_shared_from_this;

class QMenu;

namespace pv {
namespace views {
namespace trace {

/**
 * The Flag class represents items on the @ref Ruler that mark important
 * events on the timeline to the user. They are editable and thus non-static.
 */
class Flag : public TimeMarker, public enable_shared_from_this<Flag>
{
	Q_OBJECT

public:
	static const QColor FillColor;

public:
	/**
	 * Constructor.
	 * @param view A reference to the view that owns this cursor pair.
	 * @param time The time to set the flag to.
	 * @param text The text of the marker.
	 */
	Flag(View &view, const pv::util::Timestamp& time, const QString &text);

	/**
	 * Copy constructor.
	 */
	Flag(const Flag &flag);

	/**
	 * Returns true if the item is visible and enabled.
	 */
	virtual bool enabled() const override;

	/**
	 * Gets the text to show in the marker.
	 */
	virtual QString get_text() const override;

	virtual pv::widgets::Popup* create_popup(QWidget *parent) override;

	virtual QMenu* create_header_context_menu(QWidget *parent) override;

	virtual void delete_pressed() override;

	QRectF label_rect(const QRectF &rect) const override;

private Q_SLOTS:
	void on_delete();

	void on_text_changed(const QString &text);

private:
	QString text_;
};

} // namespace trace
} // namespace views
} // namespace pv

#endif // PULSEVIEW_PV_VIEWS_TRACEVIEW_FLAG_HPP
