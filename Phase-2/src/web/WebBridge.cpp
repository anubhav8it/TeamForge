#include "WebBridge.h"

#include "SampleData.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QStringList>
#include <QTimer>

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <string>

namespace webbridge {

namespace {

using emscripten::val;

// The app's runtime data folder inside the browser's (in-memory) file system.
const QString kDataDir = QStringLiteral("/home/web_user/teamforge/data");

// Files exchanged with the server: the shared data, the seed stamp (so seeding behaves exactly as
// on the desktop) and this account's own session. Logs and backup folders stay in the browser.
QStringList syncedFiles()
{
    QStringList names = sampledata::files();
    names << QStringLiteral(".sample-data") << QStringLiteral("session.json");
    return names;
}

struct SyncState
{
    QString dir;
    QHash<QString, QByteArray> known; // file name -> SHA-256 of the content the server has
};

SyncState& state()
{
    static SyncState s;
    return s;
}

QByteArray digest(const QByteArray& bytes)
{
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
}

bool isFunction(const val& value)
{
    return value.typeOf().as<std::string>() == "function";
}

// One pass: hand every changed file to the page. Upload, retry and conflict handling are the
// page's job; a file is offered again only after its content changes again.
void checkForChanges()
{
    SyncState& s = state();
    if (s.dir.isEmpty())
        return;
    const val upload = val::global("TF_upload");
    if (!isFunction(upload))
        return; // opened without the TeamForge page: run locally only
    for (const QString& name : syncedFiles()) {
        QFile file(s.dir + QLatin1Char('/') + name);
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QByteArray bytes = file.readAll();
        const QByteArray hash = digest(bytes);
        if (s.known.value(name) == hash)
            continue;
        s.known.insert(name, hash);
        upload(name.toStdString(), bytes.toStdString());
    }
}

} // namespace

QString prepareDataDirectory()
{
    SyncState& s = state();
    s.dir = kDataDir;
    if (!QDir().mkpath(s.dir))
        qWarning("TeamForge web: could not create %s", qPrintable(s.dir));
    qputenv("TEAMFORGE_DATA_DIR", s.dir.toUtf8());

    const val boot = val::global("TF_BOOT");
    if (boot.isUndefined() || boot.isNull()) {
        qWarning("TeamForge web: no TF_BOOT data from the page; running with local sample data only");
        return s.dir;
    }

    const val allowed = boot["allowedWorkspaces"];
    if (allowed.isString())
        qputenv("TEAMFORGE_ALLOWED_WORKSPACES", QByteArray::fromStdString(allowed.as<std::string>()));

    const val files = boot["files"];
    if (files.isUndefined() || files.isNull())
        return s.dir;
    const QStringList accepted = syncedFiles();
    const val keys = val::global("Object").call<val>("keys", files);
    const int count = keys["length"].as<int>();
    for (int i = 0; i < count; ++i) {
        const std::string key = keys[i].as<std::string>();
        const QString name = QString::fromStdString(key);
        const val content = files[key];
        if (!accepted.contains(name) || !content.isString())
            continue; // only the known file names, never a path
        const QByteArray bytes = QByteArray::fromStdString(content.as<std::string>());
        QFile file(s.dir + QLatin1Char('/') + name);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(bytes) != bytes.size()) {
            qWarning("TeamForge web: could not write %s", qPrintable(name));
            continue;
        }
        s.known.insert(name, digest(bytes)); // already on the server
    }
    return s.dir;
}

void flush()
{
    checkForChanges();
}

void startSync(QObject *owner)
{
    auto *timer = new QTimer(owner);
    timer->setInterval(700);
    QObject::connect(timer, &QTimer::timeout, timer, &checkForChanges);
    timer->start();
    // Files seeded or changed while the app started are offered on the first tick.
}

} // namespace webbridge

// Called by the page (instance.tfFlush()) before the tab closes, so the last change is offered
// for upload immediately rather than on the next timer tick.
EMSCRIPTEN_BINDINGS(teamforge_web)
{
    emscripten::function("tfFlush", &webbridge::flush);
}
