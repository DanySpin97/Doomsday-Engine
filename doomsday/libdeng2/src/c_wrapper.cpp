/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2011-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/c_wrapper.h"
#include "de/Error"
#include "de/App"
#include "de/Loop"
#include "de/Address"
#include "de/ByteRefArray"
#include "de/Block"
#include "de/LogBuffer"
#include "de/ByteOrder"
#include "de/Info"
#include <QFile>
#include <cstring>
#include <stdarg.h>

#define DENG2_COMMANDLINE()     DENG2_APP->commandLine()

void App_Timer(unsigned int milliseconds, void (*callback)(void))
{
    de::Loop::timer(de::TimeDelta::fromMilliSeconds(milliseconds), callback);
}

void App_FatalError(char const *msg)
{
    DENG2_APP->handleUncaughtException(msg);
}

void CommandLine_Alias(char const *longname, char const *shortname)
{
    DENG2_COMMANDLINE().alias(longname, shortname);
}

int CommandLine_Count(void)
{
    return DENG2_COMMANDLINE().count();
}

char const *CommandLine_At(int i)
{
    DENG2_ASSERT(i >= 0);
    DENG2_ASSERT(i < DENG2_COMMANDLINE().count());
    return *(DENG2_COMMANDLINE().argv() + i);
}

char const *CommandLine_PathAt(int i)
{
    DENG2_COMMANDLINE().makeAbsolutePath(i);
    return CommandLine_At(i);
}

static int argLastMatch = 0; // used only in ArgCheck/ArgNext (not thread-safe)

char const *CommandLine_Next(void)
{
    if(!argLastMatch || argLastMatch >= CommandLine_Count() - 1)
    {
        // No more arguments following the last match.
        return 0;
    }
    return CommandLine_At(++argLastMatch);
}

char const *CommandLine_NextAsPath(void)
{
    if(!argLastMatch || argLastMatch >= CommandLine_Count() - 1)
    {
        // No more arguments following the last match.
        return 0;
    }
    DENG2_COMMANDLINE().makeAbsolutePath(argLastMatch + 1);
    return CommandLine_Next();
}

int CommandLine_Check(char const *check)
{
    return argLastMatch = DENG2_COMMANDLINE().check(check);
}

int CommandLine_CheckWith(char const *check, int num)
{
    return argLastMatch = DENG2_COMMANDLINE().check(check, num);
}

int CommandLine_Exists(char const *check)
{
    return DENG2_COMMANDLINE().has(check);
}

int CommandLine_IsOption(int i)
{
    return DENG2_COMMANDLINE().isOption(i);
}

int CommandLine_IsMatchingAlias(char const *original, char const *originalOrAlias)
{
    return DENG2_COMMANDLINE().matches(original, originalOrAlias);
}

void LogBuffer_Flush(void)
{
    de::LogBuffer::appBuffer().flush();
}

void LogBuffer_Clear(void)
{
    de::LogBuffer::appBuffer().clear();
}

void LogBuffer_EnableStandardOutput(int enable)
{
	de::LogBuffer::appBuffer().enableStandardOutput(enable != 0);
}

static void logFragmentPrinter(duint32 metadata, char const *fragment)
{
    static std::string currentLogLine;

    currentLogLine += fragment;

    std::string::size_type pos;
    while((pos = currentLogLine.find('\n')) != std::string::npos)
    {
        LOG().enter(metadata, currentLogLine.substr(0, pos).c_str());
        currentLogLine.erase(0, pos + 1);
    }
}

void LogBuffer_Msg(char const *text)
{
    logFragmentPrinter(de::LogEntry::Generic | de::LogEntry::Message, text);
}

void LogBuffer_Printf(unsigned int metadata, char const *format, ...)
{
    // Automatically apply the generic domain if not specified.
    if(!(metadata & de::LogEntry::DomainMask))
    {
        metadata |= de::LogEntry::Generic;
    }

    // If this level is not enabled, just ignore.
    if(!de::LogBuffer::appBuffer().isEnabled(metadata)) return;

    // Validate the level.
    de::LogEntry::Level logLevel = de::LogEntry::Level(metadata & de::LogEntry::LevelMask);
    if(logLevel < de::LogEntry::XVerbose || logLevel > de::LogEntry::Critical)
    {
        logLevel = de::LogEntry::Message;
    }

    char buffer[2048];
    va_list args;
    va_start(args, format);
    vsprintf(buffer, format, args); /// @todo unsafe
    va_end(args);

    logFragmentPrinter(logLevel | (metadata & de::LogEntry::ContextMask), buffer);
}

Info *Info_NewFromString(char const *utf8text)
{
    try
    {
        return reinterpret_cast<Info *>(new de::Info(QString::fromUtf8(utf8text)));
    }
    catch(de::Error const &er)
    {
        LOG_WARNING(er.asText());
        return 0;
    }
}

Info *Info_NewFromFile(char const *nativePath)
{
    try
    {
        QScopedPointer<de::Info> info(new de::Info);
        info->parseNativeFile(QString::fromUtf8(nativePath));
        return reinterpret_cast<Info *>(info.take());
    }
    catch(de::Error const &er)
    {
        LOG_WARNING(er.asText());
        return 0;
    }
}

void Info_Delete(Info *info)
{
    if(info)
    {
        DENG2_SELF(Info, info);
        delete self;
    }
}

int Info_FindValue(Info *info, char const *path, char *buffer, size_t bufSize)
{
    if(!info) return false;

    DENG2_SELF(Info, info);
    de::Info::Element const *element = self->findByPath(path);
    if(!element || !element->isKey()) return false;
    QString value = static_cast<de::Info::KeyElement const *>(element)->value();
    if(buffer)
    {
        qstrncpy(buffer, value.toUtf8().constData(), uint(bufSize));
        return true;
    }
    else
    {
        // Just return the size of the value.
        return value.size();
    }
}

int UnixInfo_GetConfigValue(char const *configFile, char const *key, char *dest, size_t destLen)
{
    de::UnixInfo &info = de::App::unixInfo();

    // "paths" is the only config file currently being used.
    if(!qstrcmp(configFile, "paths"))
    {
        de::NativePath foundValue;
        if(info.path(key, foundValue))
        {
            qstrncpy(dest, foundValue.toString().toUtf8().constData(), uint(destLen));
            return true;
        }
    }
    else if(!qstrcmp(configFile, "defaults"))
    {
        de::String foundValue;
        if(info.defaults(key, foundValue))
        {
            qstrncpy(dest, foundValue.toUtf8().constData(), uint(destLen));
            return true;
        }
    }
    return false;
}

dint16 LittleEndianByteOrder_ToForeignInt16(dint16 value)
{
    DENG2_ASSERT(sizeof(dint16) == sizeof(de::dint16));
    return de::littleEndianByteOrder.toForeign(de::dint16(value));
}

dint32 LittleEndianByteOrder_ToForeignInt32(dint32 value)
{
    DENG2_ASSERT(sizeof(dint32) == sizeof(de::dint32));
    return de::littleEndianByteOrder.toForeign(de::dint32(value));
}

dint64 LittleEndianByteOrder_ToForeignInt64(dint64 value)
{
    DENG2_ASSERT(sizeof(dint64) == sizeof(de::dint64));
    return de::littleEndianByteOrder.toForeign(de::dint64(value));
}

duint16 LittleEndianByteOrder_ToForeignUInt16(duint16 value)
{
    DENG2_ASSERT(sizeof(duint16) == sizeof(de::duint16));
    return de::littleEndianByteOrder.toForeign(de::duint16(value));
}

duint32 LittleEndianByteOrder_ToForeignUInt32(duint32 value)
{
    DENG2_ASSERT(sizeof(duint32) == sizeof(de::duint32));
    return de::littleEndianByteOrder.toForeign(de::duint32(value));
}

duint64 LittleEndianByteOrder_ToForeignUInt64(duint64 value)
{
    DENG2_ASSERT(sizeof(duint64) == sizeof(de::duint64));
    return de::littleEndianByteOrder.toForeign(de::duint64(value));
}

dfloat LittleEndianByteOrder_ToForeignFloat(dfloat value)
{
    DENG2_ASSERT(sizeof(dfloat) == sizeof(de::dfloat));
    return de::littleEndianByteOrder.toForeign(de::dfloat(value));
}

ddouble LittleEndianByteOrder_ToForeignDouble(ddouble value)
{
    DENG2_ASSERT(sizeof(ddouble) == sizeof(de::ddouble));
    return de::littleEndianByteOrder.toForeign(de::ddouble(value));
}

dint16 LittleEndianByteOrder_ToNativeInt16(dint16 value)
{
    DENG2_ASSERT(sizeof(dint16) == sizeof(de::dint16));
    return de::littleEndianByteOrder.toNative(de::dint16(value));
}

dint32 LittleEndianByteOrder_ToNativeInt32(dint32 value)
{
    DENG2_ASSERT(sizeof(dint32) == sizeof(de::dint32));
    return de::littleEndianByteOrder.toNative(de::dint32(value));
}

dint64 LittleEndianByteOrder_ToNativeInt64(dint64 value)
{
    DENG2_ASSERT(sizeof(dint64) == sizeof(de::dint64));
    return de::littleEndianByteOrder.toNative(de::dint64(value));
}

duint16 LittleEndianByteOrder_ToNativeUInt16(duint16 value)
{
    DENG2_ASSERT(sizeof(duint16) == sizeof(de::duint16));
    return de::littleEndianByteOrder.toNative(de::duint16(value));
}

duint32 LittleEndianByteOrder_ToNativeUInt32(duint32 value)
{
    DENG2_ASSERT(sizeof(duint32) == sizeof(de::duint32));
    return de::littleEndianByteOrder.toNative(de::duint32(value));
}

duint64 LittleEndianByteOrder_ToNativeUInt64(duint64 value)
{
    DENG2_ASSERT(sizeof(duint64) == sizeof(de::duint64));
    return de::littleEndianByteOrder.toNative(de::duint64(value));
}

dfloat LittleEndianByteOrder_ToNativeFloat(dfloat value)
{
    DENG2_ASSERT(sizeof(dfloat) == sizeof(de::dfloat));
    return de::littleEndianByteOrder.toNative(de::dfloat(value));
}

ddouble LittleEndianByteOrder_ToNativeDouble(ddouble value)
{
    DENG2_ASSERT(sizeof(ddouble) == sizeof(de::ddouble));
    return de::littleEndianByteOrder.toNative(de::ddouble(value));
}
