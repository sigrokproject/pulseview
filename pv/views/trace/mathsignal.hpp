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

#ifndef PULSEVIEW_PV_VIEWS_TRACE_MATHSIGNAL_HPP
#define PULSEVIEW_PV_VIEWS_TRACE_MATHSIGNAL_HPP

#include <QComboBox>
#include <QDialog>
#include <QString>
#include <QTimer>

#include <pv/data/mathsignal.hpp>
#include <pv/views/trace/analogsignal.hpp>

using std::shared_ptr;

namespace pv {
namespace views {
namespace trace {

class MathEditDialog : public QDialog
{
	Q_OBJECT

public:
	MathEditDialog(pv::Session &session, shared_ptr<pv::data::MathSignal> math_signal,
		QWidget *parent = nullptr);

private Q_SLOTS:
	void accept();
	void reject();

private:
	pv::Session &session_;
	shared_ptr<pv::data::MathSignal> math_signal_;
	QString expression_, old_expression_;
};


class MathSignal : public AnalogSignal
{
	Q_OBJECT

public:
	MathSignal(pv::Session &session, shared_ptr<data::SignalBase> base);

protected:
	void populate_popup_form(QWidget *parent, QFormLayout *form);

	shared_ptr<pv::data::MathSignal> math_signal_;

private Q_SLOTS:
	void on_expression_changed(const QString &text);
	void on_sample_count_changed(const QString &text);

	void on_edit_clicked();

private:
	QLineEdit *expression_edit_;
	QComboBox *sample_count_cb_, *sample_rate_cb_;
	QString sample_count_text_, sample_rate_text_;
	QTimer delayed_expr_updater_, delayed_count_updater_, delayed_rate_updater_;
};

} // namespace trace
} // namespace views
} // namespace pv

#endif // PULSEVIEW_PV_VIEWS_TRACE_MATHSIGNAL_HPP
