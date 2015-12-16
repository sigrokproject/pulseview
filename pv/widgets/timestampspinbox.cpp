/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2015 Jens Steinhauser <jens.steinhauser@gmail.com>
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

#include "timestampspinbox.hpp"

#include <QLineEdit>
#include <QRegExp>

namespace pv {
namespace widgets {

TimestampSpinBox::TimestampSpinBox(QWidget* parent)
	: QAbstractSpinBox(parent)
	, precision_(9)
	, stepsize_("1e-6")
{
	connect(this, SIGNAL(editingFinished()), this, SLOT(on_editingFinished()));

	updateEdit();
}

void TimestampSpinBox::stepBy(int steps)
{
	setValue(value_ + steps * stepsize_);
}

QAbstractSpinBox::StepEnabled TimestampSpinBox::stepEnabled() const
{
	return QAbstractSpinBox::StepUpEnabled | QAbstractSpinBox::StepDownEnabled;
}

unsigned TimestampSpinBox::precision() const
{
	return precision_;
}

void TimestampSpinBox::setPrecision(unsigned precision)
{
	precision_ = precision;
	updateEdit();
}

const pv::util::Timestamp& TimestampSpinBox::singleStep() const
{
	return stepsize_;
}

void TimestampSpinBox::setSingleStep(const pv::util::Timestamp& step)
{
	stepsize_ = step;
}

const pv::util::Timestamp& TimestampSpinBox::value() const
{
	return value_;
}

QSize TimestampSpinBox::minimumSizeHint() const
{
	const QFontMetrics fm(fontMetrics());
	const int l = round(value_).str().size() + precision_ + 10;
	const int w = fm.width(QString(l, '0'));
	const int h = lineEdit()->minimumSizeHint().height();
	return QSize(w, h);
}

void TimestampSpinBox::setValue(const pv::util::Timestamp& val)
{
	if (val == value_)
		return;

	value_ = val;
	updateEdit();
	Q_EMIT valueChanged(value_);
}

void TimestampSpinBox::on_editingFinished()
{
	if (!lineEdit()->isModified())
		return;
	lineEdit()->setModified(false);

	QRegExp re(R"(\s*([-+]?)\s*([0-9]+\.?[0-9]*).*)");

	if (re.exactMatch(text())) {
		QStringList captures = re.capturedTexts();
		captures.removeFirst(); // remove entire match
		QString str = captures.join("");
		setValue(pv::util::Timestamp(str.toStdString()));
	} else {
		// replace the malformed entered string with the old value
		updateEdit();
	}
}

void TimestampSpinBox::updateEdit()
{
	QString newtext = pv::util::format_time_si(
		value_, pv::util::SIPrefix::none, precision_);
	lineEdit()->setText(newtext);
}

} // widgets
} // pv
