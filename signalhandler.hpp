/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2013 Adam Reichold
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

#ifndef SIGNALHANDLER_HPP
#define SIGNALHANDLER_HPP

#include <QObject>

class QSocketNotifier;

class SignalHandler : public QObject
{
	Q_OBJECT

public:
	static bool prepare_signals();

public:
	explicit SignalHandler(QObject* parent = nullptr);

Q_SIGNALS:
	void int_received();
	void term_received();

private Q_SLOTS:
	void on_socket_notifier_activated();

private:
	static void handle_signals(int sig_number);

private:
	QSocketNotifier* socket_notifier_;

private:
	static int sockets_[2];
};

#endif // SIGNALHANDLER_HPP
