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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PULSEVIEW_PV_WIDGETS_TIMESTAMPSPINBOX_HPP
#define PULSEVIEW_PV_WIDGETS_TIMESTAMPSPINBOX_HPP

#include "pv/util.hpp"

#include <QAbstractSpinBox>

namespace pv {
namespace widgets {

class TimestampSpinBox : public QAbstractSpinBox
{
	Q_OBJECT

	Q_PROPERTY(unsigned precision
		READ precision
		WRITE setPrecision)

	// Needed because of some strange behaviour of the Qt4 MOC that would add
	// a reference to a 'staticMetaObject' member of 'pv::util' (the namespace)
	// if pv::util::Timestamp is used directly in the Q_PROPERTY macros below.
	// Didn't happen with the Qt5 MOC in this case, however others have had
	// similar problems with Qt5: https://bugreports.qt.io/browse/QTBUG-37519
	typedef pv::util::Timestamp Timestamp;

	Q_PROPERTY(Timestamp singleStep
		READ singleStep
		WRITE setSingleStep)

	Q_PROPERTY(Timestamp value
		READ value
		WRITE setValue
		NOTIFY valueChanged
		USER true)

public:
	TimestampSpinBox(QWidget* parent = nullptr);

	void stepBy(int steps) override;

	StepEnabled stepEnabled() const override;

	unsigned precision() const;
	void setPrecision(unsigned precision);

	const pv::util::Timestamp& singleStep() const;
	void setSingleStep(const pv::util::Timestamp& step);

	const pv::util::Timestamp& value() const;

	QSize minimumSizeHint() const override;

public Q_SLOTS:
	void setValue(const pv::util::Timestamp& val);

Q_SIGNALS:
	void valueChanged(const pv::util::Timestamp&);

private Q_SLOTS:
	void on_editingFinished();

private:
	unsigned precision_;
	pv::util::Timestamp stepsize_;
	pv::util::Timestamp value_;

	void updateEdit();
};

}  // namespace widgets
}  // namespace pv

#endif // PULSEVIEW_PV_WIDGETS_TIMESTAMPSPINBOX_HPP
