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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef PULSEVIEW_PV_VIEW_VIEW_H
#define PULSEVIEW_PV_VIEW_VIEW_H

#include <stdint.h>

#include <vector>

#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>

#include <QAbstractScrollArea>
#include <QSizeF>

#include "cursorpair.h"

namespace pv {

class SigSession;

namespace view {

class Header;
class Ruler;
class Trace;
class Viewport;

class View : public QAbstractScrollArea {
	Q_OBJECT

private:
	static const double MaxScale;
	static const double MinScale;

	static const int RulerHeight;

	static const int MaxScrollValue;

public:
	static const int SignalHeight;
	static const int SignalMargin;
	static const int SignalSnapGridSize;

	static const QColor CursorAreaColour;

	static const QSizeF LabelPadding;

public:
	explicit View(SigSession &session, QWidget *parent = 0);

	SigSession& session();
	const SigSession& session() const;

	/**
	 * Returns the view time scale in seconds per pixel.
	 */
	double scale() const;

	/**
	 * Returns the time offset of the left edge of the view in
	 * seconds.
	 */
	double offset() const;
	int v_offset() const;

	void zoom(double steps);
	void zoom(double steps, int offset);

	void zoom_fit();

	void zoom_one_to_one();

	/**
	 * Sets the scale and offset.
	 * @param scale The new view scale in seconds per pixel.
	 * @param offset The view time offset in seconds.
	 */
	void set_scale_offset(double scale, double offset);

	std::vector< boost::shared_ptr<Trace> > get_traces() const;

	std::list<boost::weak_ptr<SelectableItem> > selected_items() const;

	/**
	 * Returns true if cursors are displayed. false otherwise.
	 */
	bool cursors_shown() const;

	/**
	 * Shows or hides the cursors.
	 */
	void show_cursors(bool show = true);

	/**
	 * Moves the cursors to a convenient position in the view.
	 */
	void centre_cursors();

	/**
	 * Returns a reference to the pair of cursors.
	 */
	CursorPair& cursors();

	/**
	 * Returns a reference to the pair of cursors.
	 */
	const CursorPair& cursors() const;

	const QPoint& hover_point() const;

	void normalize_layout();

	void update_viewport();

signals:
	void hover_point_changed();

	void signals_moved();

	void selection_changed();

	void scale_offset_changed();

private:
	void get_scroll_layout(double &length, double &offset) const;
	
	void set_zoom(double scale, int offset);

	void update_scroll();

	void update_layout();

	static bool compare_trace_v_offsets(
		const boost::shared_ptr<pv::view::Trace> &a,
		const boost::shared_ptr<pv::view::Trace> &b);

private:
	bool eventFilter(QObject *object, QEvent *event);

	bool viewportEvent(QEvent *e);

	void resizeEvent(QResizeEvent *e);

private slots:

	void h_scroll_value_changed(int value);
	void v_scroll_value_changed(int value);

	void signals_changed();
	void data_updated();

	void marker_time_changed();

	void on_signals_moved();

	void on_geometry_updated();

private:
	SigSession &_session;

	Viewport *_viewport;
	Ruler *_ruler;
	Header *_header;

	uint64_t _data_length;

	/// The view time scale in seconds per pixel.
	double _scale;

	/// The view time offset in seconds.
	double _offset;

	int _v_offset;
	bool _updating_scroll;

	bool _show_cursors;
	CursorPair _cursors;

	QPoint _hover_point;
};

} // namespace view
} // namespace pv

#endif // PULSEVIEW_PV_VIEW_VIEW_H
