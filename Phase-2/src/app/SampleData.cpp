#include "SampleData.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>

namespace sampledata {

namespace {

const QString kStamp = QStringLiteral(".sample-data");

QByteArray readAll(const QString& path, bool *ok)
{
    QFile file(path);
    *ok = file.open(QIODevice::ReadOnly);
    return *ok ? file.readAll() : QByteArray();
}

// Hash over "<name>:<sha256>;" for each sample file, so renaming or editing any one changes it.
QByteArray sampleHash(const QString& sampleDir, bool *ok)
{
    QByteArray combined;
    for (const QString& name : files()) {
        const QByteArray bytes = readAll(sampleDir + QLatin1Char('/') + name, ok);
        if (!*ok)
            return {};
        combined += name.toUtf8() + ':' + QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex() + ';';
    }
    return QCryptographicHash::hash(combined, QCryptographicHash::Sha256).toHex();
}

bool writeFile(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(bytes) == bytes.size();
}

Result replaceAll(const QString& dataDir, const QString& sampleDir, const QByteArray& hash)
{
    Result result;
    const QString backupDir = dataDir + QStringLiteral("/backup-")
                              + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    QStringList backedUp;
    for (const QString& name : files()) {
        bool ok = false;
        const QByteArray sample = readAll(sampleDir + QLatin1Char('/') + name, &ok);
        const QString target = dataDir + QLatin1Char('/') + name;
        bool hadTarget = false;
        const QByteArray current = readAll(target, &hadTarget);
        if (hadTarget && current != sample) {
            if (!QDir().mkpath(backupDir) || !writeFile(backupDir + QLatin1Char('/') + name, current))
                return {false, false, QStringLiteral("Could not back up %1 to %2").arg(name, backupDir), {}};
            backedUp.append(name);
        }
        if (!ok || !writeFile(target, sample))
            return {false, false, QStringLiteral("Could not write %1").arg(target), {}};
    }
    if (!writeFile(dataDir + QLatin1Char('/') + kStamp, hash + '\n'))
        return {false, false, QStringLiteral("Could not write the sample stamp in %1").arg(dataDir), {}};
    result.replaced = true;
    if (backedUp.isEmpty()) {
        result.message = QStringLiteral("Runtime data seeded from the bundled samples");
    } else {
        result.backupDir = backupDir;
        result.message = QStringLiteral("Runtime data updated to the current samples; previous %1 kept in %2")
                             .arg(backedUp.join(QStringLiteral(", ")), backupDir);
    }
    return result;
}

} // namespace

QString defaultSampleDirectory()
{
    return QStringLiteral(":/qt/qml/TeamForge/data");
}

Result ensureSeeded(const QString& dataDir, const QString& sampleDir)
{
    bool ok = false;
    const QByteArray hash = sampleHash(sampleDir, &ok);
    if (!ok)
        return {false, false, QStringLiteral("Bundled sample data not found in %1").arg(sampleDir), {}};
    if (!QDir().mkpath(dataDir))
        return {false, false, QStringLiteral("Cannot create data directory %1").arg(dataDir), {}};

    bool hasStamp = false;
    const QByteArray stamp = readAll(dataDir + QLatin1Char('/') + kStamp, &hasStamp).trimmed();
    if (!hasStamp || stamp != hash)
        return replaceAll(dataDir, sampleDir, hash);

    QStringList restored;
    for (const QString& name : files()) {
        const QString target = dataDir + QLatin1Char('/') + name;
        if (QFile::exists(target))
            continue;
        const QByteArray sample = readAll(sampleDir + QLatin1Char('/') + name, &ok);
        if (!ok || !writeFile(target, sample))
            return {false, false, QStringLiteral("Could not restore %1").arg(target), {}};
        restored.append(name);
    }
    Result result;
    result.message = restored.isEmpty() ? QStringLiteral("Runtime data matches the bundled samples' version")
                                        : QStringLiteral("Restored %1 from the samples").arg(restored.join(QStringLiteral(", ")));
    return result;
}

Result restore(const QString& dataDir, const QString& sampleDir)
{
    bool ok = false;
    const QByteArray hash = sampleHash(sampleDir, &ok);
    if (!ok)
        return {false, false, QStringLiteral("Bundled sample data not found in %1").arg(sampleDir), {}};
    if (!QDir().mkpath(dataDir))
        return {false, false, QStringLiteral("Cannot create data directory %1").arg(dataDir), {}};
    return replaceAll(dataDir, sampleDir, hash);
}

} // namespace sampledata
