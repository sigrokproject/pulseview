/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2020 Soeren Apel <soeren@apelpie.net>
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

#include "mathsignal.hpp"

#include <pv/session.hpp>

namespace pv {
namespace data {

void MathSignalAnalog::on_capture_state_changed(int state)
{
	if (state == Session::Running)
		begin_generation();

	// Make sure we don't miss any input samples, just in case
	if (state == Session::Stopped)
		gen_input_cond_.notify_one();
}

void MathSignalAnalog::on_data_received()
{
	gen_input_cond_.notify_one();
}

void MathSignalAnalog::on_enabled_changed()
{
	QString disabled_signals;
	if (!all_input_signals_enabled(disabled_signals) &&
		((error_type_ == MATH_ERR_NONE) || (error_type_ == MATH_ERR_ENABLE)))
		set_error(MATH_ERR_ENABLE,
			tr("No data will be generated as %1 must be enabled").arg(disabled_signals));
	else if (disabled_signals.isEmpty() && (error_type_ == MATH_ERR_ENABLE)) {
		error_type_ = MATH_ERR_NONE;
		error_message_.clear();
	}
}

void MathSignalLogic::on_capture_state_changed(int state)
{
	if (state == Session::Running)
		begin_generation();

	// Make sure we don't miss any input samples, just in case
	if (state == Session::Stopped)
		gen_input_cond_.notify_one();
}

void MathSignalLogic::on_data_received()
{
	gen_input_cond_.notify_one();
}

void MathSignalLogic::on_enabled_changed()
{
	QString disabled_signals;
	if (!all_input_signals_enabled(disabled_signals) &&
		((error_type_ == MATH_ERR_NONE) || (error_type_ == MATH_ERR_ENABLE)))
		set_error(MATH_ERR_ENABLE,
			tr("No data will be generated as %1 must be enabled").arg(disabled_signals));
	else if (disabled_signals.isEmpty() && (error_type_ == MATH_ERR_ENABLE)) {
		error_type_ = MATH_ERR_NONE;
		error_message_.clear();
	}
}

} // namespace data
} // namespace pv
