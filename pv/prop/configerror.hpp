// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef PULSEVIEW_PROP_CONFIGERROR_HPP
#define PULSEVIEW_PROP_CONFIGERROR_HPP

#include <QMessageBox>
#include <libsigrokcxx/libsigrokcxx.hpp>

namespace pv {
namespace prop {
inline void show_config_error(QWidget *parent, const sigrok::Error &error)
{
	const bool closed = error.result == SR_ERR_DEV_CLOSED;
	QMessageBox::warning(parent, closed ?
		QObject::tr("Device unavailable") : QObject::tr("Device configuration failed"),
		closed ? QObject::tr("Device unavailable. Reconnect USB, then use Connect to Device, Scan, and select the analyzer again.") :
		QString::fromUtf8(error.what()));
}
} // namespace prop
} // namespace pv
#endif
