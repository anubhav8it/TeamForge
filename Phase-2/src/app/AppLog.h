#pragma once

#include <QDateTime>
#include <QString>
#include <QVariantList>

#include <deque>

// Lightweight local application log: a bounded in-memory list (shown in the developer
// workspace) mirrored to a text file next to the data. Nothing leaves the machine.
class AppLog
{
public:
    enum class Level { Info, Warning, Error };

    struct Entry
    {
        QDateTime time;
        Level level;
        QString category; // e.g. "startup", "profile", "project", "matching", "team", "data"
        QString message;
    };

    // Where entries are appended; empty disables the file. Rotates to <file>.1 past maxFileBytes.
    void setFile(const QString& path);
    QString file() const { return file_; }

    void add(Level level, const QString& category, const QString& message);
    void info(const QString& category, const QString& message) { add(Level::Info, category, message); }
    void warning(const QString& category, const QString& message) { add(Level::Warning, category, message); }
    void error(const QString& category, const QString& message) { add(Level::Error, category, message); }

    // Newest first: {time, level, category, message}.
    QVariantList entries() const;
    int errorCount() const;

    static QString levelName(Level level);

private:
    static constexpr std::size_t kMaxEntries = 400;
    static constexpr qint64 kMaxFileBytes = 512 * 1024;

    std::deque<Entry> entries_;
    QString file_;
};
