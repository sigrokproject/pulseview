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

#include "signalhandler.h"

#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include <QSocketNotifier>

int SignalHandler::s_sockets[2];

bool SignalHandler::prepareSignals()
{
    if(socketpair(AF_UNIX, SOCK_STREAM, 0, s_sockets) != 0)
    {
        return false;
    }

    struct sigaction sigAction;

    sigAction.sa_handler = SignalHandler::handleSignals;
    sigemptyset(&sigAction.sa_mask);
    sigAction.sa_flags = SA_RESTART;

    if(sigaction(SIGINT, &sigAction, 0) != 0)
    {
        close(s_sockets[0]);
        close(s_sockets[1]);

        return false;
    }

    if(sigaction(SIGTERM, &sigAction, 0) != 0)
    {
        close(s_sockets[0]);
        close(s_sockets[1]);

        return false;
    }

    return true;
}

SignalHandler::SignalHandler(QObject* parent) : QObject(parent),
    m_socketNotifier(0)
{
    m_socketNotifier = new QSocketNotifier(s_sockets[1], QSocketNotifier::Read, this);
    connect(m_socketNotifier, SIGNAL(activated(int)), SLOT(on_socketNotifier_activated()));
}

void SignalHandler::on_socketNotifier_activated()
{
    m_socketNotifier->setEnabled(false);

    int sigNumber;
    read(s_sockets[1], &sigNumber, sizeof(int));

    switch(sigNumber)
    {
    case SIGINT:
        emit sigIntReceived();
        break;
    case SIGTERM:
        emit sigTermReceived();
        break;
    }

    m_socketNotifier->setEnabled(true);
}

void SignalHandler::handleSignals(int sigNumber)
{
    write(s_sockets[0], &sigNumber, sizeof(int));
}
