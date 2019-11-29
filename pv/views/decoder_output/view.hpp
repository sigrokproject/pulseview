/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2019 Soeren Apel <soeren@apelpie.net>
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

#ifndef PULSEVIEW_PV_VIEWS_DECODEROUTPUT_VIEW_HPP
#define PULSEVIEW_PV_VIEWS_DECODEROUTPUT_VIEW_HPP

#include <QComboBox>

#include <pv/views/viewbase.hpp>

namespace pv {

class Session;

namespace views {

namespace decoder_output {

class View : public ViewBase
{
	Q_OBJECT

public:
	explicit View(Session &session, bool is_main_view=false, QMainWindow *parent = nullptr);

	~View();

	virtual ViewType get_type() const;

	/**
	 * Resets the view to its default state after construction. It does however
	 * not reset the signal bases or any other connections with the session.
	 */
	virtual void reset_view_state();

	virtual void clear_signals();

	virtual void clear_decode_signals();
	virtual void add_decode_signal(shared_ptr<data::DecodeSignal> signal);
	virtual void remove_decode_signal(shared_ptr<data::DecodeSignal> signal);

	virtual void save_settings(QSettings &settings) const;
	virtual void restore_settings(QSettings &settings);

private Q_SLOTS:
	void on_signal_name_changed(const QString &name);

private:
	QComboBox *signal_selector_, *format_selector_;
};

} // namespace decoder_output
} // namespace views
} // namespace pv

#endif // PULSEVIEW_PV_VIEWS_DECODEROUTPUT_VIEW_HPP
