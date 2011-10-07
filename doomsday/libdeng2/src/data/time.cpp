/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Time"
#include "de/Date"
#include "de/String"
#include "de/Writer"
#include "de/Reader"

#include <QDataStream>

using namespace de;

duint64 Time::Delta::asMilliSeconds() const
{
    return duint64(_seconds * 1000);
}

Time::Delta Time::Delta::operator + (const ddouble& d) const
{
    return _seconds + d;
}

Time::Delta Time::Delta::operator - (const ddouble& d) const
{
    return _seconds - d;
}

Time::Time() : _time(QDateTime::currentDateTime())
{}

bool Time::operator < (const Time& t) const
{
    return (_time < t._time);
}

bool Time::operator == (const Time& t) const
{
    return (_time == t._time);
}

Time Time::operator + (const Delta& delta) const
{
    Time result = *this;
    result._time.addMSecs(delta.asMilliSeconds());
    return result;
}

Time& Time::operator += (const Delta& delta)
{
    _time.addMSecs(delta.asMilliSeconds());
    return *this;
}

Time::Delta Time::operator - (const Time& earlierTime) const
{
    return earlierTime._time.msecsTo(_time) / 1000.0;
}

String Time::asText() const
{
    return _time.toString("yyyy-MM-dd hh:mm:ss.zzz");
}

Date Time::asDate() const
{
    return Date(*this);
}

void Time::operator >> (Writer& to) const
{
    Block bytes;
    QDataStream s(&bytes, QIODevice::WriteOnly);
    s << _time;
    to << bytes;
}

void Time::operator << (Reader& from)
{
    Block bytes;
    from >> bytes;
    QDataStream s(bytes);
    s >> _time;
}

QTextStream& de::operator << (QTextStream& os, const Time& t)
{
    os << t.asText();
    return os;
}
