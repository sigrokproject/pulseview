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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef PULSEVIEW_PV_VIEW_FLAG_HPP
#define PULSEVIEW_PV_VIEW_FLAG_HPP

#include <memory>

#include "timemarker.hpp"

class QMenu;

namespace pv {
namespace view {

class Flag : public TimeMarker,
	public std::enable_shared_from_this<pv::view::Flag>
{
	Q_OBJECT

public:
	static const QColor FillColour;

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
	bool enabled() const;

	/**
	 * Gets the text to show in the marker.
	 */
	QString get_text() const;

	pv::widgets::Popup* create_popup(QWidget *parent);

	QMenu* create_context_menu(QWidget *parent);

	void delete_pressed();

private Q_SLOTS:
	void on_delete();

	void on_text_changed(const QString &text);

private:
	QString text_;
};

} // namespace view
} // namespace pv

#endif // PULSEVIEW_PV_VIEW_FLAG_HPP
