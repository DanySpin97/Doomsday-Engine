/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/LegacyCore"
#include "de/LegacyNetwork"
#include "de/LogBuffer"

#include "../core/callbacktimer.h"

#include <QCoreApplication>
#include <QList>
#include <QTimer>
#include <QDebug>
#include <string>

using namespace de;

LegacyCore* LegacyCore::_appCore;

/**
 * @internal Private instance data for LegacyCore.
 */
struct LegacyCore::Instance
{
    struct Loop {
        int interval;
        void (*func)(void);
        Loop() : interval(1), func(0) {}
    };
    QList<Loop> loopStack;
    void (*terminateFunc)(const char*);

    App* app;
    LegacyNetwork network;
    Loop loop;
    LogBuffer logBuffer;

    /// Pointer returned to callers, see LegacyCore::logFileName().
    std::string logName;

    /// Current log output line being formed.
    std::string currentLogLine;

    Instance() : terminateFunc(0), app(0) {}
    ~Instance() {}
};

LegacyCore::LegacyCore(App* dengApp)
{
    _appCore = this;
    d = new Instance;

    // Construct a new core application (must have one for the event loop).
    d->app = dengApp;

    connect(d->app, SIGNAL(uncaughtException(QString)), this, SLOT(handleUncaughtException(QString)));

    // The global log buffer will be available for the entire runtime of deng2.
    LogBuffer::setAppBuffer(d->logBuffer);
#ifdef DENG2_DEBUG
    d->logBuffer.enable(Log::DEBUG);
#endif
    //d->logBuffer.enable(Log::TRACE);
}

LegacyCore::~LegacyCore()
{
    stop();

    delete d;   
    _appCore = 0;
}

LegacyCore& LegacyCore::instance()
{
    DENG2_ASSERT(_appCore != 0);
    DENG2_ASSERT(_appCore->d != 0);
    return *_appCore;
}

LegacyNetwork& LegacyCore::network()
{
    return instance().d->network;
}

void LegacyCore::setLoopFunc(void (*func)(void))
{
    LOG_DEBUG("Loop function changed from %p set to %p.") << dintptr(d->loop.func) << dintptr(func);

    // Set up a timer to periodically call the provided callback function.
    d->loop.func = func;

    if(func)
    {
        // Start the periodic callback calls.
        QTimer::singleShot(d->loop.interval, this, SLOT(callback()));
    }
}

void LegacyCore::pushLoop()
{
    d->loopStack.append(d->loop);
}

void LegacyCore::popLoop()
{
    if(d->loopStack.isEmpty())
    {
        LOG_CRITICAL("Pop from empty loop stack.");
        return;
    }

    d->loop = d->loopStack.last();
    d->loopStack.removeLast();

    LOG_DEBUG("Loop function popped, now %p.") << dintptr(d->loop.func);

    if(d->loop.func)
    {
        // Start the periodic callback calls.
        QTimer::singleShot(d->loop.interval, this, SLOT(callback()));
    }
}

int LegacyCore::runEventLoop()
{
    LOG_MSG("Starting LegacyCore event loop...");

    // Run the Qt event loop. In the future this will be replaced by the
    // application's main Qt event loop, where deng2 will hook into.
    int code = d->app->exec();

    LOG_MSG("Event loop exited with code %i.") << code;
    return code;
}

void LegacyCore::setLoopRate(int freqHz)
{
    const int oldInterval = d->loop.interval;
    d->loop.interval = qMax(1, 1000/freqHz);

    if(oldInterval != d->loop.interval)
    {
        LOG_DEBUG("Loop interval changed to %i ms.") << d->loop.interval;
    }
}

void LegacyCore::stop(int exitCode)
{
    d->app->exit(exitCode);
}

void LegacyCore::timer(duint32 milliseconds, void (*func)(void))
{
    // The timer will delete itself after it's triggered.
    internal::CallbackTimer* timer = new internal::CallbackTimer(func, this);
    timer->start(milliseconds);
}

void LegacyCore::setLogFileName(const char *nativeFilePath)
{
    d->logName = nativeFilePath;
    d->logBuffer.setOutputFile(nativeFilePath);
}

const char *LegacyCore::logFileName() const
{
    return d->logName.c_str();
}

void LegacyCore::printLogFragment(const char* text, Log::LogLevel level)
{
    d->currentLogLine += text;

    std::string::size_type pos;
    while((pos = d->currentLogLine.find('\n')) != std::string::npos)
    {
        LOG().enter(level, d->currentLogLine.substr(0, pos).c_str());
        d->currentLogLine.erase(0, pos + 1);
    }
}

void LegacyCore::setTerminateFunc(void (*func)(const char*))
{
    d->terminateFunc = func;
}

void LegacyCore::callback()
{
    if(d->loop.func)
    {
        d->loop.func();

        QTimer::singleShot(d->loop.interval, this, SLOT(callback()));
    }
}

void LegacyCore::handleUncaughtException(QString message)
{
    LOG_CRITICAL(message);

    if(d->terminateFunc) d->terminateFunc(message.toUtf8().constData());
}
