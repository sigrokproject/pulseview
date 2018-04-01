/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2018 Soeren Apel <soeren@apelpie.net>
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

#ifndef PULSEVIEW_PV_LOGGING_HPP
#define PULSEVIEW_PV_LOGGING_HPP

#include "globalsettings.hpp"

#include <mutex>

#include <QtGlobal>
#include <QObject>
#include <QString>
#include <QStringList>

using std::mutex;

namespace pv {

class Logging : public QObject, public GlobalSettingsInterface
{
	Q_OBJECT

public:
	enum LogSource {
		LogSource_pv,
		LogSource_sr,
		LogSource_srd
	};

	static const int MAX_BUFFER_SIZE;

public:
	~Logging();
	void init();

	int get_log_level() const;
	void set_log_level(int level);

	QString get_log() const;

	void log(const QString &text, int source);

	static void log_pv(QtMsgType type, const QMessageLogContext &context, const QString &msg);

	static int log_sr(void *cb_data, int loglevel, const char *format, va_list args);

#ifdef ENABLE_DECODE
	static int log_srd(void *cb_data, int loglevel, const char *format, va_list args);
#endif

private:
	void on_setting_changed(const QString &key, const QVariant &value);

Q_SIGNALS:
	void logged_text(QString s);

private:
	int buffer_size_;
	QStringList buffer_;
	mutable mutex log_mutex_;
};

extern Logging logging;

} // namespace pv

#endif // PULSEVIEW_PV_LOGGING_HPP
