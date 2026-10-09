#include "AppLog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QVariantMap>

#include <algorithm>

void AppLog::setFile(const QString& path)
{
    file_ = path;
}

QString AppLog::levelName(Level level)
{
    switch (level) {
    case Level::Info: return QStringLiteral("info");
    case Level::Warning: return QStringLiteral("warning");
    case Level::Error: return QStringLiteral("error");
    }
    return QStringLiteral("info");
}

void AppLog::add(Level level, const QString& category, const QString& message)
{
    Entry entry{QDateTime::currentDateTime(), level, category, message};
    entries_.push_back(entry);
    if (entries_.size() > kMaxEntries)
        entries_.pop_front();

    // Best effort: a log that cannot be written never interrupts the user.
    if (file_.isEmpty() || !QFileInfo(QFileInfo(file_).absolutePath()).isDir())
        return;
    if (QFileInfo(file_).size() > kMaxFileBytes) {
        QFile::remove(file_ + QStringLiteral(".1"));
        QFile::rename(file_, file_ + QStringLiteral(".1"));
    }
    QFile out(file_);
    if (!out.open(QIODevice::Append | QIODevice::Text))
        return;
    QTextStream stream(&out);
    stream << entry.time.toString(Qt::ISODateWithMs) << ' ' << levelName(level).toUpper() << " ["
           << category << "] " << message << '\n';
}

QVariantList AppLog::entries() const
{
    QVariantList list;
    list.reserve(static_cast<qsizetype>(entries_.size()));
    for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
        list.append(QVariantMap{{QStringLiteral("time"), it->time.toString(QStringLiteral("HH:mm:ss"))},
                                {QStringLiteral("date"), it->time.toString(Qt::ISODate)},
                                {QStringLiteral("level"), levelName(it->level)},
                                {QStringLiteral("category"), it->category},
                                {QStringLiteral("message"), it->message}});
    }
    return list;
}

int AppLog::errorCount() const
{
    return static_cast<int>(std::count_if(entries_.begin(), entries_.end(),
                                          [](const Entry& e) { return e.level == Level::Error; }));
}
