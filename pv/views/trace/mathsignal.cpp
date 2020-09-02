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
#include <QDialogButtonBox>
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


MathEditDialog::MathEditDialog(pv::Session &session,
	shared_ptr<pv::data::MathSignal> math_signal, QWidget *parent) :
	QDialog(parent),
	session_(session),
	math_signal_(math_signal),
	expression_(math_signal->get_expression()),
	old_expression_(math_signal->get_expression())
{
	setWindowTitle(tr("Math Expression Editor"));

	// Create the rest of the dialog
	QDialogButtonBox *button_box = new QDialogButtonBox(
		QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

	QVBoxLayout* root_layout = new QVBoxLayout(this);
//	root_layout->addLayout(tab_layout);
	root_layout->addWidget(button_box);

	connect(button_box, SIGNAL(accepted()), this, SLOT(accept()));
	connect(button_box, SIGNAL(rejected()), this, SLOT(reject()));
}

void MathEditDialog::accept()
{
	math_signal_->set_expression(expression_);
	QDialog::accept();
}

void MathEditDialog::reject()
{
	math_signal_->set_expression(old_expression_);
	QDialog::reject();
}


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

	const QIcon edit_icon(QIcon::fromTheme("edit", QIcon(":/icons/math.svg")));
	QAction *edit_action =
		expression_edit_->addAction(edit_icon, QLineEdit::TrailingPosition);

	connect(expression_edit_, SIGNAL(textEdited(QString)),
		this, SLOT(on_expression_changed(QString)));
	connect(edit_action, SIGNAL(triggered(bool)),
		this, SLOT(on_edit_clicked()));
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

void MathSignal::on_edit_clicked()
{
	MathEditDialog dlg(session_, math_signal_);

	dlg.exec();
}

} // namespace trace
} // namespace views
} // namespace pv
