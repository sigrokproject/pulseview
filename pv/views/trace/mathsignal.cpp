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

#include <extdef.h>

#include <QComboBox>
#include <QDebug>
#include <QFormLayout>
#include <QGridLayout>
#include <QLineEdit>
#include <QString>

#include "mathsignal.hpp"

#include "pv/data/signalbase.hpp"

using pv::data::SignalBase;

namespace pv {
namespace views {
namespace trace {

#define MATHSIGNAL_INPUT_TIMEOUT (2000)

MathSignal::MathSignal(
	pv::Session &session,
	shared_ptr<data::SignalBase> base) :
	AnalogSignal(session, base),
	math_signal_(dynamic_pointer_cast<pv::data::MathSignal>(base))
{
	delayed_expr_updater_.setSingleShot(true);
	delayed_expr_updater_.setInterval(MATHSIGNAL_INPUT_TIMEOUT);
	connect(&delayed_expr_updater_, &QTimer::timeout,
		this, [this]() { math_signal_->set_expression(expression_edit_->text()); });
}

void MathSignal::populate_popup_form(QWidget *parent, QFormLayout *form)
{
	AnalogSignal::populate_popup_form(parent, form);

	expression_edit_ = new QLineEdit();
	expression_edit_->setText(math_signal_->get_expression());
	connect(expression_edit_, SIGNAL(textEdited(QString)),
		this, SLOT(on_expression_changed(QString)));
	form->addRow(tr("Expression"), expression_edit_);

	sample_count_cb_ = new QComboBox();
	sample_count_cb_->setEditable(true);
	sample_count_cb_->addItem(tr("same as session"));
	sample_count_cb_->addItem(tr("100"));
	sample_count_cb_->addItem(tr("10000"));
	sample_count_cb_->addItem(tr("1000000"));
	connect(sample_count_cb_, SIGNAL(editTextChanged(QString)),
		this, SLOT(on_sample_count_changed(QString)));
	form->addRow(tr("Number of Samples"), sample_count_cb_);

	sample_rate_cb_ = new QComboBox();
	sample_rate_cb_->setEditable(true);
	sample_rate_cb_->addItem(tr("same as session"));
	sample_rate_cb_->addItem(tr("100"));
	sample_rate_cb_->addItem(tr("10000"));
	sample_rate_cb_->addItem(tr("1000000"));
	form->addRow(tr("Sample rate"), sample_rate_cb_);
}

void MathSignal::on_expression_changed(const QString &text)
{
	(void)text;

	// Restart update timer
	delayed_expr_updater_.start();
}

void MathSignal::on_sample_count_changed(const QString &text)
{
	(void)text;
}

} // namespace trace
} // namespace views
} // namespace pv
