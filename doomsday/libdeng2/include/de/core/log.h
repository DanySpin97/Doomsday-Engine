/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_LOG_H
#define LIBDENG2_LOG_H

#include "../Time"
#include "../String"
#include "../Lockable"

#include <QList>
#include <vector>
#include <string>
#include <cstdlib>

/// Macro for accessing the local log of the current thread.
#define LOG() \
    de::Log& log = de::Log::threadLog();

/// Macro for accessing the local log of the current thread and entering
/// a new log section.
#define LOG_AS(sectionName) \
    de::Log::Section __logSection = de::Log::Section(sectionName);
    
/// Macro for accessing the local log of the current thread and entering
/// a new log section with a std::string variable based name.
#define LOG_AS_STRING(str) \
    de::String __logSectionName = str; \
    LOG_AS(__logSectionName.c_str());

#define LOG_TRACE(str)      de::Log::threadLog().enter(de::Log::TRACE, str)
#define LOG_DEBUG(str)      de::Log::threadLog().enter(de::Log::DEBUG, str)
#define LOG_VERBOSE(str)    de::Log::threadLog().enter(de::Log::VERBOSE, str)
#define LOG_MSG(str)        de::Log::threadLog().enter(str)
#define LOG_INFO(str)       de::Log::threadLog().enter(de::Log::INFO, str)
#define LOG_WARNING(str)    de::Log::threadLog().enter(de::Log::WARNING, str)
#define LOG_ERROR(str)      de::Log::threadLog().enter(de::Log::ERROR, str)
#define LOG_CRITICAL(str)   de::Log::threadLog().enter(de::Log::CRITICAL, str)

#ifdef WIN32
#   undef ERROR
#endif

namespace de {

class LogBuffer;
class LogEntry;

/**
 * Logs provide means for adding log entries into the log entry buffer. The
 * thread's Log keeps track of the section stack. Each thread uses its own
 * logs.
 *
 * @ingroup core
 */
class DENG2_PUBLIC Log
{
public:
    /// Level of the log entry.
    enum LogLevel
    {
        /**
         * Trace messages are intended for low-level debugging. They should be used
         * to log which methods are entered and exited, and mark certain points within
         * methods. Intended only for developers and debug builds.
         */
        TRACE,

        /**
         * Debug messages are intended for normal debugging. They should be enabled
         * only in debug builds. An example of a debug message might be a printout of
         * a ZIP archive's file count and size once an archive has been successfully
         * opened. Intended only for developers and debug builds.
         */
        DEBUG,

        /**
         * Verbose messages should be used to log technical information that is only
         * of interest to advanced users. An example of a verbose message could be
         * the summary of all the defined object types during the launch of a game.
         * Verbose messages should not be used for anything that produces a large
         * number of log entries, such as an entry about reading the contents of a
         * file within a ZIP archive (which would be suitable for the DEBUG level).
         */
        VERBOSE,

        /**
         * Normal log entries are intended for regular users. An example: message about
         * which map is being loaded.
         */
        MESSAGE,

        /**
         * Info messages are intended for situations that are particularly noteworthy.
         * An info message should be used for instance when a script has been stopped
         * because of an uncaught exception occurred during its execution.
         */
        INFO,

        /**
         * Warning messages are reserved for recoverable error situations. A warning
         * might be logged for example when the expected resource could not be found,
         * and a fallback resource was used instead.
         */
        WARNING,

        /**
         * Error messages are intended for nonrecoverable errors. The error is grave
         * enough to cause the shutting down of the current game, but the engine can
         * still remain running.
         */
        ERROR,

        /**
         * Critical messages are intended for fatal errors that cause the engine to be
         * shut down.
         */
        CRITICAL,

        MAX_LOG_LEVELS
    };

    class DENG2_PUBLIC Section
    {
    public:
        /**
         * The Section does not take a copy of @c name, so whatever
         * it's pointing to must exist while the Section exists.
         *
         * @param name  Name of the log section.
         */
        Section(const char* name);
        ~Section();

        Log& log() const { return _log; }

    private:
        Log& _log;
        const char* _name;
    };

public:
    Log();
    virtual ~Log();

    /**
     * Begins a new section in the log. Sections can be nested.
     *
     * @param name  Name of the section. No copy of this string is made,
     *              so it must exist while the section is in use.
     */
    void beginSection(const char* name);

    /**
     * Ends the topmost section in the log.
     *
     * @param name  Name of the topmost section.
     */
    void endSection(const char* name);

    /**
     * Creates a new log entry with the default (MESSAGE) level.
     * Append the parameters of the entry using the << operator.
     *
     * @param format  Format template of the entry.
     */
    LogEntry& enter(const String& format);

    /**
     * Creates a new log entry with the specified level.
     * Append the parameters of the entry using the << operator.
     *
     * @param level   Level of the entry.
     * @param format  Format template of the entry.
     */
    LogEntry& enter(LogLevel level, const String& format);

public:
    /**
     * Returns the logger of the current thread.
     */
    static Log& threadLog();

    /**
     * Deletes the current thread's log. Threads should call this before
     * they quit.
     */
    static void disposeThreadLog();

private:
    typedef QList<const char*> SectionStack;
    SectionStack _sectionStack;

    LogEntry* _throwawayEntry;
};

/**
 * An entry to be stored in the log entry buffer. Log entries are created with
 * Log::enter().
 *
 * @ingroup core
 */
class DENG2_PUBLIC LogEntry
{
public:
    /**
     * Argument for a log entry.
     *
     * @ingroup core
     */
    class DENG2_PUBLIC Arg : public String::IPatternArg
    {
    public:
        /// The wrong type is used in accessing the value. @ingroup errors
        DENG2_ERROR(TypeError)

        enum Type {
            INTEGER,
            FLOATING_POINT,
            STRING
        };

        /**
         * Base class for classes that support adding to the arguments.
         */
        class Base {
        public:
            /// Attempted conversion from unsupported type.
            DENG2_ERROR(TypeError)

        public:
            virtual ~Base() {}

            virtual Type logEntryArgType() const = 0;
            virtual dint64 asInt64() const {
                throw TypeError("LogEntry::Arg::Base", "dint64 not supported");
            }
            virtual ddouble asDouble() const {
                throw TypeError("LogEntry::Arg::Base", "ddouble not supported");
            }
            virtual String asText() const {
                throw TypeError("LogEntry::Arg::Base", "String not supported");
            }
        };

    public:
        Arg(dint i) : _type(INTEGER) { _data.intValue = i; }
        Arg(duint i) : _type(INTEGER) { _data.intValue = i; }
        Arg(long unsigned int i) : _type(INTEGER) { _data.intValue = i; }
        Arg(duint64 i) : _type(INTEGER) { _data.intValue = dint64(i); }
        Arg(dint64 i) : _type(INTEGER) { _data.intValue = i; }
        Arg(ddouble d) : _type(FLOATING_POINT) { _data.floatValue = d; }
        Arg(const void* p) : _type(INTEGER) { _data.intValue = dint64(p); }
        Arg(const char* s) : _type(STRING) {
            _data.stringValue = new String(s);
        }
        Arg(const String& s) : _type(STRING) {
            _data.stringValue = new String(s);
        }
        Arg(const Base& arg) : _type(arg.logEntryArgType()) {
            switch(_type) {
            case INTEGER:
                _data.intValue = arg.asInt64();
                break;
            case FLOATING_POINT:
                _data.floatValue = arg.asDouble();
                break;
            case STRING:
                _data.stringValue = new String(arg.asText());
                break;
            }
        }
        ~Arg() {
            if(_type == STRING) {
                delete _data.stringValue;
            }
        }

        Type type() const { return _type; }
        dint64 intValue() const {
            DENG2_ASSERT(_type == INTEGER);
            return _data.intValue;
        }
        ddouble floatValue() const {
            DENG2_ASSERT(_type == FLOATING_POINT);
            return _data.floatValue;
        }
        QString stringValue() const {
            DENG2_ASSERT(_type == STRING);
            return *_data.stringValue;
        }

        // Implements String::IPatternArg.
        ddouble asNumber() const {
            if(_type == INTEGER) {
                return ddouble(_data.intValue);
            }
            else if(_type == FLOATING_POINT) {
                return _data.floatValue;
            }
            throw TypeError("Log::Arg::asNumber",
                "String argument cannot be used as a number");
        }
        String asText() const {
            if(_type == STRING) {
                return *_data.stringValue;
            }
            throw TypeError("Log::Arg::asText",
                "Number argument cannot be used a string");
        }

    private:
        Type _type;
        union Data {
            dint64 intValue;
            ddouble floatValue;
            String* stringValue;
        } _data;
    };

public:
    enum Flag
    {
        /// In simple mode, only print the actual message contents,
        /// without metadata.
        Simple = 0x1,

        /// Use escape sequences to format the entry with text styles
        /// (for graphical output).
        Styled = 0x2
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    /// The format string has incorrect syntax. @ingroup errors
    DENG2_ERROR(IllegalFormatError)

public:
    LogEntry(Log::LogLevel level, const String& section, const String& format);
    ~LogEntry();

    /// Appends a new argument to the entry.
    template <typename ValueType>
    inline LogEntry& operator << (const ValueType& v) {
        if(!_disabled) {
            _args.push_back(new Arg(v));
        }
        return *this;
    }

    /// Returns the timestamp of the entry.
    Time when() const { return _when; }

    Log::LogLevel level() const { return _level; }

    /// Converts the log entry to a string.
    String asText(const Flags& flags = 0) const;

    /// Make this entry print without metadata.
    LogEntry& simple() {
        _defaultFlags |= Simple;
        return *this;
    }

private:
    void advanceFormat(String::const_iterator& i) const;

private:
    Time _when;
    Log::LogLevel _level;
    String _section;
    String _format;
    Flags _defaultFlags;
    bool _disabled;

    typedef std::vector<Arg*> Args;
    Args _args;
};

QTextStream& operator << (QTextStream& stream, const LogEntry::Arg& arg);

Q_DECLARE_OPERATORS_FOR_FLAGS(LogEntry::Flags)

} // namespace de

#endif /* LIBDENG2_LOG_H */
