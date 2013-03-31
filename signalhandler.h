/*

Copyright 2013 Adam Reichold

This file is part of qpdfview.

qpdfview is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

qpdfview is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with qpdfview.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef SIGNALHANDLER_H
#define SIGNALHANDLER_H

#include <QObject>

class QSocketNotifier;

class SignalHandler : public QObject
{
    Q_OBJECT

public:
    static bool prepareSignals();

    explicit SignalHandler(QObject* parent = 0);

signals:
    void sigIntReceived();
    void sigTermReceived();

private slots:
    void on_socketNotifier_activated();

private:
    static int s_sockets[2];

    static void handleSignals(int sigNumber);

    QSocketNotifier* m_socketNotifier;

};

#endif // SIGNALHANDLER_H
