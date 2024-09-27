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

#ifndef PULSEVIEW_PV_DATA_MATHSIGNAL_HPP
#define PULSEVIEW_PV_DATA_MATHSIGNAL_HPP

#include <QString>

#include <pv/session.hpp>
#include <pv/data/analog.hpp>
#include <pv/data/analogsegment.hpp>
#include <pv/data/logic.hpp>
#include <pv/data/logicsegment.hpp>
#include <pv/data/mathsignalbase.hpp>

namespace pv {
namespace data {

class MathSignalAnalog : public MathSignalBase<Analog>
{
	Q_OBJECT
	Q_PROPERTY(QString expression READ get_expression WRITE set_expression NOTIFY expression_changed)

Q_SIGNALS:
	void expression_changed(QString expression);

private Q_SLOTS:
	void on_capture_state_changed(int state);
	void on_data_received();

	void on_enabled_changed();

public:
	MathSignalAnalog(pv::Session &session) :
		MathSignalBase<Analog>(session)
	{
	}
};

class MathSignalLogic : public MathSignalBase<Logic>
{
	Q_OBJECT
	Q_PROPERTY(QString expression READ get_expression WRITE set_expression NOTIFY expression_changed)

Q_SIGNALS:
	void expression_changed(QString expression);

private Q_SLOTS:
	void on_capture_state_changed(int state);
	void on_data_received();

	void on_enabled_changed();

public:
	MathSignalLogic(pv::Session &session) :
		MathSignalBase<Logic>(session)
	{
	}
};

} // namespace data
} // namespace pv

#endif // PULSEVIEW_PV_DATA_MATHSIGNAL_HPP
