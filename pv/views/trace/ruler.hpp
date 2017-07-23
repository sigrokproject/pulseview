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

#ifndef PULSEVIEW_PV_VIEWS_TRACEVIEW_RULER_HPP
#define PULSEVIEW_PV_VIEWS_TRACEVIEW_RULER_HPP

#include <functional>
#include <memory>

#include <boost/optional.hpp>

#include "marginwidget.hpp"
#include <pv/util.hpp>

using std::function;
using std::pair;
using std::shared_ptr;
using std::vector;

namespace RulerTest {
struct tick_position_test_0;
struct tick_position_test_1;
struct tick_position_test_2;
}

namespace pv {
namespace views {
namespace trace {

class TimeItem;
class ViewItem;

/**
 * The Ruler class manages and displays the time scale above the trace canvas.
 * It may also contain @ref TimeItem instances used to identify or highlight
 * time-related information.
 */
class Ruler : public MarginWidget
{
	Q_OBJECT

	friend struct RulerTest::tick_position_test_0;
	friend struct RulerTest::tick_position_test_1;
	friend struct RulerTest::tick_position_test_2;

private:
	/// Height of the ruler in multipes of the text height
	static const float RulerHeight;

	static const int MinorTickSubdivision;

	/// Height of the hover arrow in multiples of the text height
	static const float HoverArrowSize;

public:
	Ruler(View &parent);

public:
	QSize sizeHint() const override;

	/**
	 * The extended area that the header widget would like to be sized to.
	 * @remarks This area is the area specified by sizeHint, extended by
	 * the area to overlap the viewport.
	 */
	QSize extended_size_hint() const override;

	/**
	 * Formats a timestamp depending on its distance to another timestamp.
	 *
	 * Heuristic function, useful when multiple timestamps should be put side by
	 * side. The function procedes in the following order:
	 *   - If 't' is zero, "0" is returned.
	 *   - If 'unit' is 'TimeUnit::Samples', 'pv::util::format_time_si_adjusted()'
	 *     is used to format 't'.
	 *   - If a zoomed out view is detected (determined by 'precision' and
	 *     'distance'), 'pv::util::format_time_minutes() is used.
	 *   - For timestamps "near the origin" (determined by 'distance'),
	 *    'pv::util::format_time_si_adjusted()' is used.
	 *   - If none of the previous was true, 'pv::util::format_time_minutes()'
	 *     is used again.
	 *
	 * @param distance The distance between the timestamp to format and
	 *        an adjacent one.
	 * @param t The value to format
	 * @param prefix The SI prefix to use.
	 * @param unit The representation of the timestamp value.
	 * @param precision The number of digits after the decimal separator.
	 * @param sign Whether or not to add a sign also for positive numbers.
	 *
	 * @return The formated value.
	 */
	static QString format_time_with_distance(
		const pv::util::Timestamp& distance,
		const pv::util::Timestamp& t,
		pv::util::SIPrefix prefix = pv::util::SIPrefix::unspecified,
		pv::util::TimeUnit unit = pv::util::TimeUnit::Time,
		unsigned precision = 0,
		bool sign = true);

private:
	/**
	 * Gets the time items.
	 */
	vector< shared_ptr<ViewItem> > items() override;

	/**
	 * Gets the first view item which has a label that contains @c pt .
	 * @param pt the point to search with.
	 * @return the view item that has been found, or and empty
	 *   @c shared_ptr if no item was found.
	 */
	shared_ptr<ViewItem> get_mouse_over_item(const QPoint &pt) override;

	void paintEvent(QPaintEvent *event) override;

	void mouseDoubleClickEvent(QMouseEvent *event) override;

	/**
	 * Draw a hover arrow under the cursor position.
	 * @param p The painter to draw into.
	 * @param text_height The height of a single text ascent.
	 */
	void draw_hover_mark(QPainter &p, int text_height);

	int calculate_text_height() const;

	struct TickPositions
	{
		vector<pair<double, QString>> major;
		vector<double> minor;
	};

	/**
	 * Holds the tick positions so that they don't have to be recalculated on
	 * every redraw. Set by 'paintEvent()' when needed.
	 */
	boost::optional<TickPositions> tick_position_cache_;

	/**
	 * Calculates the major and minor tick positions.
	 *
	 * @param major_period The period between the major ticks.
	 * @param offset The time at the left border of the ruler.
	 * @param scale The scale in seconds per pixel.
	 * @param width the Width of the ruler.
	 * @param format_function A function used to format the major tick times.
	 * @return An object of type 'TickPositions' that contains the major tick
	 *         positions together with the labels at that ticks, and the minor
	 *         tick positions.
	 */
	static TickPositions calculate_tick_positions(
		const pv::util::Timestamp& major_period,
		const pv::util::Timestamp& offset,
		const double scale,
		const int width,
		function<QString(const pv::util::Timestamp&)> format_function);

protected:
	void resizeEvent(QResizeEvent*) override;

private Q_SLOTS:
	void hover_point_changed(const QPoint &hp);

	// Resets the 'tick_position_cache_'.
	void invalidate_tick_position_cache();
};

} // namespace trace
} // namespace views
} // namespace pv

#endif // PULSEVIEW_PV_VIEWS_TRACEVIEW_RULER_HPP
