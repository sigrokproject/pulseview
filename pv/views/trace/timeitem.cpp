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

#include "signal.hpp"
#include "timeitem.hpp"
#include "view.hpp"

namespace pv {
namespace views {
namespace trace {

TimeItem::TimeItem(View &view) :
	view_(view) {
}

void TimeItem::drag_by(const QPoint &delta)
{
	if (snapping_disabled_) {
		set_time(view_.offset() + (drag_point_.x() + delta.x() - 0.5) *
			view_.scale());
	} else {
		int64_t sample_num = view_.get_nearest_level_change(drag_point_ + delta);

		if (sample_num > -1)
			set_time(sample_num / view_.get_signal_under_mouse_cursor()->base()->get_samplerate());
		else
			set_time(view_.offset() + (drag_point_.x() + delta.x() - 0.5) *
				view_.scale());
	}
}

const pv::util::Timestamp TimeItem::delta(const pv::util::Timestamp& other) const
{
	return other - time();
}


bool TimeItem::is_snapping_disabled() const
{
	return snapping_disabled_;
}

} // namespace trace
} // namespace views
} // namespace pv
