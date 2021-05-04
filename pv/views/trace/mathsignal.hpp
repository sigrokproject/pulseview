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

#include <memory>
#include <vector>
#include <functional>

#include <QComboBox>
#include <QDialog>
#include <QPlainTextEdit>
#include <QString>
#include <QTimer>

#include <pv/data/mathsignal.hpp>
#include <pv/views/trace/analogsignal.hpp>

using std::pair;
using std::shared_ptr;
using std::string;
using std::vector;
using std::function;

namespace pv {
namespace views {
namespace trace {

class MathEditDialog : public QDialog
{
	Q_OBJECT

private:
	static const vector< pair<string, string> > Examples;

public:
	MathEditDialog(pv::Session &session, function<void(QString)> math_signal_expr_setter,
		QString old_expression, QWidget *parent = nullptr);

	void set_expr(const QString &expr);

private Q_SLOTS:
	void accept();
	void reject();

private:
	pv::Session &session_;
	shared_ptr<pv::data::MathSignalAnalog> math_signal_;
	function<void(QString)> math_signal_expr_setter_;
	QString old_expression_;

	QPlainTextEdit *expr_edit_;
};


class MathSignalAnalog : public AnalogSignal
{
	Q_OBJECT

public:
	MathSignalAnalog(pv::Session &session, shared_ptr<data::SignalBase> base);

protected:
	void populate_popup_form(QWidget *parent, QFormLayout *form);

	shared_ptr<pv::data::MathSignalAnalog> math_signal_;

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

class MathSignalLogic : public LogicSignal
{
	Q_OBJECT

public:
	MathSignalLogic(pv::Session &session, shared_ptr<data::SignalBase> base);

protected:
	void populate_popup_form(QWidget *parent, QFormLayout *form);

	shared_ptr<pv::data::MathSignalLogic> math_signal_;

private Q_SLOTS:
	void on_expression_changed(const QString &text);

	void on_edit_clicked();

private:
	QLineEdit *expression_edit_;
	QComboBox *sample_count_cb_, *sample_rate_cb_;
	QTimer delayed_expr_updater_, delayed_count_updater_, delayed_rate_updater_;
};

} // namespace trace
} // namespace views
} // namespace pv

#endif // PULSEVIEW_PV_VIEWS_TRACE_MATHSIGNAL_HPP
