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

#ifndef LIBDENG2_LOGBUFFER_H
#define LIBDENG2_LOGBUFFER_H

#include "../Log"
#ifdef DENG2_FS_AVAILABLE
#include "../File"
#endif

#include <QObject>
#include <QTimer>
#include <QList>

namespace de {

class LogEntry;

/**
 * Buffer for log entries. Log entries get creates in thread-specific logs and
 * then get flushed to the LogBuffer.
 *
 * The application owns one of these.
 *
 * @ingroup core
 */
class LogBuffer : public QObject
#ifdef DENG2_FS_AVAILABLE
    , OBSERVES(File, Deletion)
#endif
{
    Q_OBJECT

public:
    typedef QList<const LogEntry*> Entries;

public:
    /**
     * Constructs a new log buffer. By default log levels starting with MESSAGE
     * are enabled. Output goes to stdout/stderr. @see enableStandardOutput().
     *
     * @param maxEntryCount  Maximum number of entries to keep in memory.
     */
    LogBuffer(duint maxEntryCount = 1000);

    virtual ~LogBuffer();

    void setMaxEntryCount(duint maxEntryCount);

    /**
     * Adds an entry to the buffer. The buffer gets ownership.
     *
     * @param entry  Entry to add.
     */
    void add(LogEntry* entry);

    void clear();

    /**
     * Returns the number of entries stored in the buffer.
     */
    dsize size() const;

    /**
     * Returns the latest entries from the buffer. Note that when new entries
     * are added the older entries may be deleted. The entries returned in @a
     * entries should either be used immediately, or copies should be made in
     * the case they're needed later on.
     *
     * @param entries  The entries are placed here. The first entry of the
     *                 array is the latest entry in the buffer.
     * @param count    Number of entries to get. If zero, all entries are
     *                 returned.
     */
    void latestEntries(Entries& entries, int count = 0) const;

    /**
     * Enables log entries at or over a level. When a level is disabled, the
     * entries will not be added to the log entry buffer.
     */
    void enable(Log::LogLevel overLevel = Log::MESSAGE);

    /**
     * Disables the log. @see enable()
     */
    void disable() { enable(Log::MAX_LOG_LEVELS); }

    bool enabled(Log::LogLevel overLevel = Log::MESSAGE) const {
        return _enabledOverLevel <= overLevel;
    }

    /**
     * Enables or disables standard output of log messages. When enabled,
     * log entries are written with simple formatting to the standard
     * output and error streams when the buffer is flushed.
     *
     * @param yes  @c true or @c false.
     */
    void enableStandardOutput(bool yes = true) {
        _standardOutput = yes;
    }

    /**
     * Sets the path of the file used for writing log entries to.
     *
     * @param path  Path of the file.
     */
    void setOutputFile(const String& path);

#ifdef DENG2_FS_AVAILABLE
    // Observes File deletion.
    void fileBeingDeleted(const File& file);
#endif

public:
    /**
     * Sets the application's global log buffer. This is available to all.
     * Ownership is not transferred, so whoever created the buffer is
     * reponsible for deleting it after no one needs the log any more.
     *
     * @param appBuffer  LogBuffer instance.
     */
    static void setAppBuffer(LogBuffer& appBuffer);

    static LogBuffer& appBuffer();

public slots:
    /**
     * Flushes all unflushed entries to the defined outputs.
     */
    void flush();

private:
    typedef QList<LogEntry*> EntryList;

    dint _enabledOverLevel;
    dint _maxEntryCount;
    bool _standardOutput;
#ifdef DENG2_FS_AVAILABLE
    File* _outputFile;
#endif
    EntryList _entries;
    EntryList _toBeFlushed;
    Time _lastFlushedAt;
    QTimer* _autoFlushTimer;

    /// The globally available application buffer.
    static LogBuffer* _appBuffer;
};

} // namespace de

#endif // LIBDENG2_LOGBUFFER_H
