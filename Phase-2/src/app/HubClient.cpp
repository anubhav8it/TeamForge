#include "HubClient.h"

#include "core/Errors.h"

#include <algorithm>

#include <QBuffer>
#include <QDateTime>
#include <QImage>
#include <QImageReader>
#include <QJSEngine>
#include <QJsonDocument>
#include <QQmlEngine>

#ifdef Q_OS_WASM
#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>
#else
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QNetworkRequest>
#endif

namespace hub = teamforge::hub;

namespace {

std::string ss(const QString& text)
{
    return text.toStdString();
}

QString qs(const std::string& text)
{
    return QString::fromStdString(text);
}

int utcOffsetMinutes()
{
    return QDateTime::currentDateTime().offsetFromUtc() / 60;
}

QString errorOf(const QVariant& data)
{
    return data.toMap().value(QStringLiteral("error")).toString();
}

constexpr int kAvatarSide = 160;
constexpr int kAvatarMaxBytes = 80 * 1024;

} // namespace

// =========================================================================== browser transport

#ifdef Q_OS_WASM
namespace {

QPointer<HubClient>& hubTarget()
{
    static QPointer<HubClient> target;
    return target;
}

// (C++ macro text until built: no regular expressions or empty quotes in these.)
EM_JS(int, tf_hub_available, (), {
    return typeof window.TF_hub === 'function' ? 1 : 0;
});

// Texts are passed as UTF-8 bytes with their lengths and decoded here.
EM_JS(void, tf_hub_request, (int id, const char *method, int methodLength, const char *path, int pathLength,
                             const char *body, int bodyLength), {
    var text = function (at, length) { return new TextDecoder().decode(HEAPU8.slice(at, at + length)); };
    var done = function (status, answer) { Module.tfHubReply(id, status, answer); };
    if (typeof window.TF_hub !== 'function') {
        done(0, JSON.stringify({ error: 'This page cannot reach the TeamForge server.' }));
        return;
    }
    window.TF_hub(text(method, methodLength), text(path, pathLength), text(body, bodyLength)).then(function (answer) {
        done(answer.status, answer.text);
    }, function () {
        done(0, JSON.stringify({ error: 'The server could not be reached. Check your connection.' }));
    });
});

// A hidden <input type=file> for a profile picture; the bytes come back through tfAvatarLoaded.
EM_JS(void, tf_pick_avatar, (), {
    var input = document.createElement('input');
    input.type = 'file';
    input.accept = 'image/png,image/jpeg,image/webp,image/gif,image/bmp';
    input.style.display = 'none';
    input.onchange = function () {
        var file = input.files && input.files[0];
        input.remove();
        if (!file)
            return;
        if (file.size > 15 * 1024 * 1024) {
            Module.tfAvatarLoaded(new Uint8Array(0));
            return;
        }
        var decodable = ['image/jpeg', 'image/png', 'image/bmp', 'image/gif'].indexOf(file.type) >= 0;
        var give = function (blob) {
            blob.arrayBuffer().then(function (buffer) {
                Module.tfAvatarLoaded(new Uint8Array(buffer));
            }).catch(function () {
                Module.tfAvatarLoaded(new Uint8Array(0));
            });
        };
        if (!decodable && window.createImageBitmap) {
            createImageBitmap(file).then(function (bitmap) {
                var canvas = document.createElement('canvas');
                canvas.width = bitmap.width;
                canvas.height = bitmap.height;
                canvas.getContext('2d').drawImage(bitmap, 0, 0);
                canvas.toBlob(function (png) { give(png || file); }, 'image/png');
            }).catch(function () { give(file); });
            return;
        }
        give(file);
    };
    document.body.appendChild(input);
    input.click();
});

void hubReply(int id, int status, const std::string& text)
{
    if (HubClient *target = hubTarget().data())
        target->deliver(id, status, QByteArray::fromStdString(text));
}

void avatarLoaded(const emscripten::val& data)
{
    HubClient *target = hubTarget().data();
    if (target == nullptr)
        return;
    const auto length = data["length"].as<unsigned>();
    QByteArray bytes(static_cast<qsizetype>(length), Qt::Uninitialized);
    if (length > 0)
        emscripten::val(emscripten::typed_memory_view(length, reinterpret_cast<unsigned char *>(bytes.data())))
            .call<void>("set", data);
    target->avatarBytes(bytes);
}

} // namespace

EMSCRIPTEN_BINDINGS(teamforge_hub)
{
    emscripten::function("tfHubReply", &hubReply);
    emscripten::function("tfAvatarLoaded", &avatarLoaded);
}
#endif

// =========================================================================== HubClient

HubClient::HubClient(QObject *parent)
    : QObject(parent)
{
    chat_ = new HubChatModel(this);
#ifdef Q_OS_WASM
    hubTarget() = this;
#else
    network_ = new QNetworkAccessManager(this);
    network_->setCookieJar(new QNetworkCookieJar(network_));
    serverUrl_ = qEnvironmentVariable("TEAMFORGE_SERVER").trimmed();
    while (serverUrl_.endsWith(QLatin1Char('/')))
        serverUrl_.chop(1);
#endif
    refreshTimer_.setInterval(30000);
    connect(&refreshTimer_, &QTimer::timeout, this, &HubClient::refresh);
    if (browser() && available())
        QTimer::singleShot(0, this, &HubClient::refresh);
}

HubClient::~HubClient() = default;

bool HubClient::browser()
{
#ifdef Q_OS_WASM
    return true;
#else
    return false;
#endif
}

bool HubClient::available() const
{
#ifdef Q_OS_WASM
    return tf_hub_available() != 0;
#else
    return true; // the desktop app signs in to a server first
#endif
}

int HubClient::pendingDecisions() const
{
    const QVariantMap pending = summary_.value(QStringLiteral("pendingDecisions")).toMap();
    return pending.value(QStringLiteral("team")).toInt() + pending.value(QStringLiteral("event")).toInt();
}

QVariantList HubClient::recentNotifications() const
{
    QVariantList list;
    for (const hub::Notification& n : notifications_.items())
        list.append(QVariantMap{{QStringLiteral("id"), n.id}, {QStringLiteral("kind"), qs(n.kind)},
                                {QStringLiteral("title"), qs(n.title)}, {QStringLiteral("body"), qs(n.body)},
                                {QStringLiteral("link"), qs(n.link)}, {QStringLiteral("read"), n.read},
                                {QStringLiteral("createdAt"), n.createdAt}});
    return list;
}

void HubClient::setRoute(const QString& route)
{
    const QString cleaned = route.isEmpty() ? QStringLiteral("home") : route;
    if (cleaned == route_)
        return;
    route_ = cleaned;
    emit routeChanged();
}

QString HubClient::anonymityNotice()
{
    return qs(hub::anonymityNotice());
}

// --------------------------------------------------------------------------- transport

void HubClient::request(const QString& method, const QString& path, const QVariant& body, Callback done)
{
    const int id = nextRequest_++;
    pending_.insert(id, std::move(done));
    const QByteArray json = body.isValid() ? QJsonDocument::fromVariant(body).toJson(QJsonDocument::Compact) : QByteArray();
#ifdef Q_OS_WASM
    const QByteArray m = method.toUtf8();
    const QByteArray p = path.toUtf8();
    tf_hub_request(id, m.constData(), int(m.size()), p.constData(), int(p.size()), json.constData(), int(json.size()));
#else
    if (serverUrl_.isEmpty()) {
        QTimer::singleShot(0, this, [this, id] {
            deliver(id, 0, QByteArrayLiteral(R"({"error":"Sign in to a TeamForge server first."})"));
        });
        return;
    }
    QNetworkRequest req(QUrl(serverUrl_ + path));
    req.setRawHeader("X-TeamForge", "1");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    req.setTransferTimeout(60000);
    QNetworkReply *reply = nullptr;
    if (method == QLatin1String("POST")) {
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        reply = network_->post(req, json.isEmpty() ? QByteArrayLiteral("{}") : json);
    } else {
        reply = network_->get(req);
    }
    connect(reply, &QNetworkReply::finished, this, [this, reply, id] {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray bytes = reply->readAll();
        if (status == 0)
            bytes = QByteArrayLiteral(R"({"error":"The server could not be reached. Check the address and your connection."})");
        reply->deleteLater();
        deliver(id, status, bytes);
    });
#endif
}

void HubClient::deliver(int requestId, int status, const QByteArray& body)
{
    const Callback done = pending_.take(requestId);
    QVariant data = QJsonDocument::fromJson(body).toVariant();
    if (!data.isValid())
        data = QVariantMap{{QStringLiteral("error"), status == 0 ? QStringLiteral("The server could not be reached.")
                                                                 : QStringLiteral("Unexpected answer from the server (%1).").arg(status)}};
    handleStatus(status);
    if (done)
        done(status, data);
}

void HubClient::handleStatus(int status)
{
    if (status == 401 && connected_) {
        connected_ = false;
        refreshTimer_.stop();
        chat_->close();
        statusText_ = browser() ? tr("You have been signed out. Reload the page to sign in again.")
                                : tr("Signed out. Sign in again to use the Community.");
        emit stateChanged();
    }
}

void HubClient::callJs(const QJSValue& callback, int status, const QVariant& data)
{
    if (!callback.isCallable())
        return;
    QJSEngine *engine = qjsEngine(this);
    if (engine == nullptr)
        return;
    QJSValue fn = callback;
    const QJSValue result = fn.call({QJSValue(status), engine->toScriptValue(data)});
    if (result.isError())
        qWarning("Hub callback failed: %s", qPrintable(result.toString()));
}

void HubClient::get(const QString& path, const QJSValue& callback)
{
    request(QStringLiteral("GET"), QStringLiteral("/api/hub") + path, QVariant(),
            [this, callback](int status, const QVariant& data) { callJs(callback, status, data); });
}

void HubClient::post(const QString& path, const QVariant& body, const QJSValue& callback)
{
    QVariant payload = body;
    if (payload.metaType() == QMetaType::fromType<QJSValue>())
        payload = payload.value<QJSValue>().toVariant();
    if (!payload.isValid() || payload.isNull())
        payload = QVariantMap();
    request(QStringLiteral("POST"), QStringLiteral("/api/hub") + path, payload,
            [this, callback, path](int status, const QVariant& data) {
                if (status >= 200 && status < 300) {
                    emit changed(path);
                    QTimer::singleShot(0, this, &HubClient::refresh);
                }
                callJs(callback, status, data);
            });
}

void HubClient::signIn(const QString& server, const QString& email, const QString& password, const QJSValue& callback)
{
#ifndef Q_OS_WASM
    QString base = server.trimmed();
    while (base.endsWith(QLatin1Char('/')))
        base.chop(1);
    const QUrl url(base);
    if (!url.isValid() || (url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https")) || url.host().isEmpty()) {
        callJs(callback, 400, QVariantMap{{QStringLiteral("error"), tr("Enter the server address, e.g. https://teamforge-geu.onrender.com")}});
        return;
    }
    if (base != serverUrl_)
        network_->setCookieJar(new QNetworkCookieJar(network_)); // another server: no old cookies
    serverUrl_ = base;
    request(QStringLiteral("POST"), QStringLiteral("/api/login"),
            QVariantMap{{QStringLiteral("email"), email.trimmed()}, {QStringLiteral("password"), password}},
            [this, callback](int status, const QVariant& data) {
                if (status == 200)
                    refresh();
                else {
                    statusText_ = errorOf(data);
                    emit stateChanged();
                }
                callJs(callback, status, data);
            });
    emit stateChanged();
#else
    Q_UNUSED(server)
    Q_UNUSED(email)
    Q_UNUSED(password)
    callJs(callback, 400, QVariantMap{{QStringLiteral("error"), tr("The web version uses the page's own sign-in.")}});
#endif
}

void HubClient::signOut()
{
    if (browser())
        return; // the page's Sign out button does that
    request(QStringLiteral("POST"), QStringLiteral("/api/logout"), QVariantMap(), [](int, const QVariant&) {});
    connected_ = false;
    refreshTimer_.stop();
    chat_->close();
    summary_.clear();
    notifications_.clear();
    statusText_.clear();
    emit stateChanged();
    emit summaryChanged();
    emit notificationsChanged();
}

void HubClient::applySummary(const QVariantMap& summary)
{
    summary_ = summary;
    const QString version = summary.value(QStringLiteral("identityVersion")).toString();
    if (!version.isEmpty() && version != identityVersion_) {
        identityVersion_ = version;
        loadIdentities();
    }
    const bool wasConnected = connected_;
    connected_ = true;
    statusText_.clear();
    if (!refreshTimer_.isActive())
        refreshTimer_.start();
    if (!wasConnected)
        emit stateChanged();
    emit summaryChanged();
}

void HubClient::loadIdentities()
{
    request(QStringLiteral("GET"), QStringLiteral("/api/hub/identities"), QVariant(), [this](int status, const QVariant& data) {
        if (status != 200)
            return;
        QHash<QString, QVariantMap> fresh;
        for (const QVariant& value : data.toMap().value(QStringLiteral("items")).toList()) {
            const QVariantMap person = value.toMap();
            const QString studentId = person.value(QStringLiteral("studentId")).toString();
            if (!studentId.isEmpty())
                fresh.insert(studentId, person);
        }
        identities_ = fresh;
        ++identityRevision_;
        emit identitiesChanged();
    });
}

QVariantMap HubClient::identityForStudent(const QString& studentId) const
{
    if (studentId.isEmpty() || !connected_)
        return {};
    // One's own identity is known from the summary even before the list arrives.
    const QVariantMap me = profile();
    if (me.value(QStringLiteral("studentId")).toString() == studentId)
        return me;
    return identities_.value(studentId);
}

QString HubClient::photo(const QString& ref)
{
    if (ref.isEmpty() || ref.startsWith(QLatin1String("data:")))
        return ref;
    if (!ref.startsWith(QLatin1String("/api/hub/photos/")))
        return {};
    const auto cached = photos_.constFind(ref);
    if (cached != photos_.constEnd())
        return *cached;
    if (!photosPending_.contains(ref)) {
        photosPending_.insert(ref);
        request(QStringLiteral("GET"), ref, QVariant(), [this, ref](int status, const QVariant& data) {
            photosPending_.remove(ref);
            if (status != 200)
                return;
            photos_.insert(ref, data.toMap().value(QStringLiteral("image")).toString());
            ++photoRevision_;
            emit photosChanged();
        });
    }
    return {};
}

void HubClient::syncIdentity(const QString& name, const QString& program, const QString& studentId)
{
    if (!connected_ || name.trimmed().isEmpty())
        return;
    post(QStringLiteral("/profile/sync"),
         QVariantMap{{QStringLiteral("name"), name}, {QStringLiteral("program"), program},
                     {QStringLiteral("studentId"), studentId}});
}

void HubClient::uploadPhoto(const QString& dataUrl, const QJSValue& callback)
{
    post(QStringLiteral("/profile/photo"), QVariantMap{{QStringLiteral("image"), dataUrl}}, callback);
}

void HubClient::removePhoto(const QJSValue& callback)
{
    post(QStringLiteral("/profile/photo/remove"), QVariantMap(), callback);
}

void HubClient::refresh()
{
    request(QStringLiteral("GET"), QStringLiteral("/api/hub/summary"), QVariant(), [this](int status, const QVariant& data) {
        if (status == 200) {
            applySummary(data.toMap());
        } else if (status == 401 && !connected_) {
            statusText_ = browser() ? tr("You have been signed out. Reload the page to sign in again.")
                                    : tr("Sign in to use the Community.");
            emit stateChanged();
        } else if (status != 401) {
            const QString text = errorOf(data);
            if (text != statusText_) {
                statusText_ = text;
                emit stateChanged();
            }
        }
    });
    request(QStringLiteral("GET"), QStringLiteral("/api/hub/notifications?pageSize=8"), QVariant(),
            [this](int status, const QVariant& data) {
                if (status != 200)
                    return;
                std::vector<hub::Notification> items;
                for (const QVariant& value : data.toMap().value(QStringLiteral("items")).toList()) {
                    const QVariantMap m = value.toMap();
                    items.push_back({m.value(QStringLiteral("id")).toLongLong(), ss(m.value(QStringLiteral("kind")).toString()),
                                     ss(m.value(QStringLiteral("title")).toString()), ss(m.value(QStringLiteral("body")).toString()),
                                     ss(m.value(QStringLiteral("link")).toString()), m.value(QStringLiteral("read")).toBool(),
                                     m.value(QStringLiteral("createdAt")).toDouble()});
                }
                notifications_.clear(); // the newest page is the truth (older ones may be deleted)
                notifications_.merge(items);
                emit notificationsChanged();
            });
}

bool HubClient::openNotification(const QVariantMap& notification)
{
    const long long id = notification.value(QStringLiteral("id")).toLongLong();
    if (!notification.value(QStringLiteral("read")).toBool() && id > 0) {
        notifications_.markRead(id);
        emit notificationsChanged();
        request(QStringLiteral("POST"), QStringLiteral("/api/hub/notifications/read"),
                QVariantMap{{QStringLiteral("ids"), QVariantList{id}}}, [this](int status, const QVariant&) {
                    if (status == 200)
                        refresh();
                });
    }
    const QString target = qs(hub::linkRoute(ss(notification.value(QStringLiteral("link")).toString())));
    if (target.isEmpty())
        return false;
    setRoute(target);
    return true;
}

void HubClient::markAllNotificationsRead()
{
    notifications_.markAllRead();
    emit notificationsChanged();
    post(QStringLiteral("/notifications/read"), QVariantMap{{QStringLiteral("all"), true}});
}

// --------------------------------------------------------------------------- rules and wording

QString HubClient::statusLabel(const QString& status) const
{
    static const QHash<QString, QString> labels{
        {QStringLiteral("pending"), tr("Pending")}, {QStringLiteral("approved"), tr("Approved")},
        {QStringLiteral("rejected"), tr("Rejected")}, {QStringLiteral("withdrawn"), tr("Withdrawn")},
        {QStringLiteral("waitlisted"), tr("Waitlisted")}, {QStringLiteral("new"), tr("New")},
        {QStringLiteral("reviewed"), tr("Reviewed")}, {QStringLiteral("interested"), tr("Interested")},
        {QStringLiteral("declined"), tr("Declined")}, {QStringLiteral("leader"), tr("Leader")},
        {QStringLiteral("admin"), tr("Admin")}, {QStringLiteral("member"), tr("Member")}};
    return labels.value(status, status);
}

QString HubClient::statusTone(const QString& status) const
{
    if (status == QLatin1String("approved") || status == QLatin1String("interested") || status == QLatin1String("leader"))
        return QStringLiteral("success");
    if (status == QLatin1String("pending") || status == QLatin1String("waitlisted") || status == QLatin1String("new"))
        return QStringLiteral("warning");
    if (status == QLatin1String("reviewed") || status == QLatin1String("admin"))
        return QStringLiteral("info");
    if (status == QLatin1String("rejected"))
        return QStringLiteral("accent");
    return QStringLiteral("muted");
}

QVariantList HubClient::reviewerActions(const QString& kind, const QString& status) const
{
    QVariantList out;
    try {
        const auto k = kind == QLatin1String("event") ? hub::ApplicationKind::Event : hub::ApplicationKind::Team;
        for (hub::ApplicationStatus to : hub::reviewerActions(k, hub::applicationStatusFrom(ss(status)))) {
            const QString value = to == hub::ApplicationStatus::Approved ? QStringLiteral("approve")
                                : to == hub::ApplicationStatus::Waitlisted ? QStringLiteral("waitlist")
                                                                           : QStringLiteral("reject");
            const QString text = to == hub::ApplicationStatus::Approved ? tr("Approve")
                               : to == hub::ApplicationStatus::Waitlisted ? tr("Waitlist") : tr("Reject");
            out.append(QVariantMap{{QStringLiteral("value"), value}, {QStringLiteral("text"), text},
                                   {QStringLiteral("status"), qs(hub::applicationStatusName(to))}});
        }
    } catch (const teamforge::TeamForgeError&) {
    }
    return out;
}

bool HubClient::canWithdraw(const QString& kind, const QString& status, bool deadlinePassed) const
{
    try {
        const auto k = kind == QLatin1String("event") ? hub::ApplicationKind::Event : hub::ApplicationKind::Team;
        return hub::canWithdraw(k, hub::applicationStatusFrom(ss(status)), deadlinePassed);
    } catch (const teamforge::TeamForgeError&) {
        return false;
    }
}

QString HubClient::teamApplyBlocker(const QVariantMap& team) const
{
    const QVariantMap mine = team.value(QStringLiteral("myApplication")).toMap();
    hub::TeamApplyState state;
    state.member = !team.value(QStringLiteral("myRole")).toString().isEmpty();
    state.recruiting = team.value(QStringLiteral("recruiting")).toBool();
    state.members = team.value(QStringLiteral("memberCount")).toInt();
    state.maxMembers = team.value(QStringLiteral("maxMembers")).toInt();
    state.hasPendingApplication = mine.value(QStringLiteral("status")).toString() == QLatin1String("pending");
    return qs(hub::teamApplyBlocker(state));
}

bool HubClient::canRemoveMember(const QString& actorRole, const QString& targetRole) const
{
    try {
        return hub::canRemoveMember(hub::teamRoleFrom(ss(actorRole)), hub::teamRoleFrom(ss(targetRole)));
    } catch (const teamforge::TeamForgeError&) {
        return false;
    }
}

bool HubClient::canChangeRoles(const QString& role) const
{
    try {
        return hub::canChangeRoles(hub::teamRoleFrom(ss(role)));
    } catch (const teamforge::TeamForgeError&) {
        return false;
    }
}

bool HubClient::canManageTeam(const QString& role) const
{
    try {
        return hub::canManageTeam(hub::teamRoleFrom(ss(role)));
    } catch (const teamforge::TeamForgeError&) {
        return false;
    }
}

QString HubClient::checkText(const QString& text, int limit, const QString& what, bool required) const
{
    try {
        hub::cleanText(ss(text), static_cast<std::size_t>(std::max(0, limit)), ss(what), required);
        return {};
    } catch (const teamforge::TeamForgeError& error) {
        return qs(error.what()) + QLatin1Char('.');
    }
}

int HubClient::characterCount(const QString& text) const
{
    return static_cast<int>(hub::characterCount(ss(text)));
}

double HubClient::now() const
{
    return QDateTime::currentMSecsSinceEpoch() / 1000.0;
}

QString HubClient::relativeTime(double epochSeconds) const
{
    const QString text = qs(hub::relativeTime(epochSeconds, now()));
    return text.isEmpty() ? dateText(epochSeconds) : text;
}

QString HubClient::dateText(double epochSeconds) const
{
    return QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(epochSeconds * 1000)).toString(QStringLiteral("d MMM yyyy"));
}

QString HubClient::deadlineText(double epochSeconds) const
{
    return qs(hub::deadlineText(epochSeconds, now()));
}

double HubClient::parseDate(const QString& text) const
{
    const QDate date = QDate::fromString(text.trimmed(), Qt::ISODate);
    if (!date.isValid())
        return 0;
    return QDateTime(date, QTime(23, 59, 59)).toMSecsSinceEpoch() / 1000.0;
}

QString HubClient::isoDate(double epochSeconds) const
{
    if (epochSeconds <= 0)
        return {};
    return QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(epochSeconds * 1000)).date().toString(Qt::ISODate);
}

// --------------------------------------------------------------------------- profile picture

QString HubClient::encodeAvatar(const QByteArray& bytes, QString *error) const
{
    QImage image;
    if (bytes.isEmpty() || !image.loadFromData(bytes)) {
        *error = tr("That file is not a picture TeamForge can read. Use a JPEG or PNG.");
        return {};
    }
    // Centre square, scaled down: a profile picture is shown at most at 96 px.
    const int side = std::min(image.width(), image.height());
    image = image.copy((image.width() - side) / 2, (image.height() - side) / 2, side, side)
                .scaled(kAvatarSide, kAvatarSide, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                .convertToFormat(QImage::Format_RGB32);
    for (const char *format : {"JPEG", "PNG"}) {
        for (int quality : {85, 70, 50}) {
            QByteArray out;
            QBuffer buffer(&out);
            buffer.open(QIODevice::WriteOnly);
            if (!image.save(&buffer, format, quality))
                break; // this build cannot write the format
            if (out.size() <= kAvatarMaxBytes)
                return QStringLiteral("data:image/%1;base64,").arg(QString::fromLatin1(format).toLower())
                     + QString::fromLatin1(out.toBase64());
            if (qstrcmp(format, "PNG") == 0)
                break;
        }
    }
    *error = tr("The picture could not be made small enough. Try another one.");
    return {};
}

QString HubClient::avatarFromFile(const QUrl& file)
{
#ifndef Q_OS_WASM
    QFile f(file.toLocalFile());
    if (!f.open(QIODevice::ReadOnly) || f.size() > 15 * 1024 * 1024) {
        emit avatarFailed(tr("That picture could not be opened (at most 15 MB)."));
        return {};
    }
    avatarBytes(f.readAll());
#else
    Q_UNUSED(file)
#endif
    return {};
}

void HubClient::avatarBytes(const QByteArray& bytes)
{
    QBuffer buffer;
    buffer.setData(bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    reader.setAutoTransform(true); // phone photos: honour the camera's rotation
    QImage image = bytes.isEmpty() ? QImage() : reader.read();
    if (image.isNull()) {
        emit avatarFailed(tr("That file is not a picture TeamForge can read. Use a JPEG or PNG."));
        return;
    }
    if (image.width() < 64 || image.height() < 64) {
        emit avatarFailed(tr("That picture is too small. Use one of at least 64 × 64 pixels."));
        return;
    }
    if (image.width() > 1024 || image.height() > 1024)
        image = image.scaled(1024, 1024, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    avatarSource_ = image.convertToFormat(QImage::Format_RGB32);
    QByteArray preview;
    QBuffer out(&preview);
    out.open(QIODevice::WriteOnly);
    if (!avatarSource_.save(&out, "JPEG", 88)) {
        preview.clear();
        out.seek(0);
        avatarSource_.save(&out, "PNG");
    }
    const QString kind = preview.startsWith("\x89PNG") ? QStringLiteral("png") : QStringLiteral("jpeg");
    emit avatarSourceReady(QStringLiteral("data:image/%1;base64,").arg(kind) + QString::fromLatin1(preview.toBase64()),
                           avatarSource_.width(), avatarSource_.height());
}

QString HubClient::cropAvatar(double x, double y, double side)
{
    if (avatarSource_.isNull()) {
        emit avatarFailed(tr("Choose a picture first."));
        return {};
    }
    const int s = std::max(1, std::min({int(side), avatarSource_.width(), avatarSource_.height()}));
    const int left = std::clamp(int(x), 0, avatarSource_.width() - s);
    const int top = std::clamp(int(y), 0, avatarSource_.height() - s);
    const QImage square = avatarSource_.copy(left, top, s, s);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    square.save(&buffer, "PNG");
    QString error;
    const QString url = encodeAvatar(bytes, &error); // centre square of a square: the crop itself
    if (url.isEmpty())
        emit avatarFailed(error);
    return url;
}

void HubClient::pickAvatar()
{
#ifdef Q_OS_WASM
    tf_pick_avatar();
#endif
    // Desktop: the page opens a FileDialog and calls avatarFromFile().
}

// =========================================================================== HubChatModel

HubChatModel::HubChatModel(HubClient *hub)
    : QAbstractListModel(hub)
    , hub_(hub)
{
}

int HubChatModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(log_.size());
}

QHash<int, QByteArray> HubChatModel::roleNames() const
{
    return {{MessageIdRole, "messageId"}, {BodyRole, "body"}, {AuthorIdRole, "authorId"}, {AuthorNameRole, "authorName"},
            {AuthorColorRole, "authorColor"}, {AuthorAvatarRole, "authorAvatar"}, {MineRole, "mine"},
            {TimeTextRole, "timeText"}, {DayLabelRole, "dayLabel"}, {StartsGroupRole, "startsGroup"},
            {StartsDayRole, "startsDay"}, {CreatedAtRole, "createdAt"}};
}

QVariant HubChatModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
        return {};
    const auto row = static_cast<std::size_t>(index.row());
    const hub::ChatMessage& m = log_.messages()[row];
    const QDateTime when = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(m.createdAt * 1000));
    switch (role) {
    case MessageIdRole: return m.id;
    case BodyRole: return qs(m.body);
    case AuthorIdRole: return m.authorId;
    case AuthorNameRole: return qs(m.authorName);
    case AuthorColorRole: return authors_.value(m.authorId).value(QStringLiteral("color"));
    case AuthorAvatarRole: return authors_.value(m.authorId).value(QStringLiteral("avatar"));
    case MineRole: return m.mine;
    case TimeTextRole: return when.toString(QStringLiteral("HH:mm"));
    case DayLabelRole: {
        const QDate day = when.date();
        const QDate today = QDate::currentDate();
        return day == today ? tr("Today") : day == today.addDays(-1) ? tr("Yesterday") : day.toString(QStringLiteral("d MMMM yyyy"));
    }
    case StartsGroupRole: return log_.startsGroup(row, utcOffsetMinutes());
    case StartsDayRole: return log_.startsDay(row, utcOffsetMinutes());
    case CreatedAtRole: return m.createdAt;
    default: return {};
    }
}

void HubChatModel::setActive(bool active)
{
    if (active_ == active)
        return;
    active_ = active;
    emit stateChanged();
    if (active_)
        markRead();
}

void HubChatModel::open(int teamId)
{
    if (teamId == teamId_ && teamId_ != 0 && error_.isEmpty())
        return;
    close();
    teamId_ = teamId;
    loading_ = true;
    const int generation = generation_;
    emit stateChanged();
    hub_->request(QStringLiteral("GET"), QStringLiteral("/api/hub/teams/%1/messages").arg(teamId), QVariant(),
                  [this, generation](int status, const QVariant& data) {
                      if (generation != generation_)
                          return;
                      loading_ = false;
                      if (status != 200) {
                          fail(status, data.toMap());
                          return;
                      }
                      const QVariantMap page = data.toMap();
                      lastReadId_ = page.value(QStringLiteral("lastReadId")).toLongLong();
                      merge(page.value(QStringLiteral("messages")).toList());
                      log_.setHasOlder(page.value(QStringLiteral("hasMore")).toBool());
                      const auto unread = log_.firstUnread(lastReadId_);
                      firstUnreadRow_ = unread ? static_cast<int>(*unread) : -1;
                      live_ = true;
                      emit stateChanged();
                      markRead();
                      poll();
                  });
}

void HubChatModel::close()
{
    ++generation_; // late replies for the old conversation are ignored
    beginResetModel();
    log_.clear();
    authors_.clear();
    endResetModel();
    teamId_ = 0;
    lastReadId_ = markedRead_ = 0;
    firstUnreadRow_ = -1;
    loading_ = loadingOlder_ = sending_ = live_ = false;
    retryDelayMs_ = 2000;
    error_.clear();
    emit stateChanged();
}

void HubChatModel::fail(int status, const QVariantMap& data)
{
    live_ = false;
    error_ = status == 403 || status == 404 ? tr("You are not a member of this team (any more).")
                                            : data.value(QStringLiteral("error")).toString();
    emit stateChanged();
}

void HubChatModel::merge(const QVariantList& messages)
{
    std::vector<hub::ChatMessage> page;
    page.reserve(static_cast<std::size_t>(messages.size()));
    for (const QVariant& value : messages) {
        const QVariantMap m = value.toMap();
        const QVariantMap author = m.value(QStringLiteral("author")).toMap();
        const long long authorId = author.value(QStringLiteral("id")).toLongLong();
        authors_.insert(authorId, QVariantMap{{QStringLiteral("color"), author.value(QStringLiteral("color"))},
                                              {QStringLiteral("avatar"), author.value(QStringLiteral("avatar"))}});
        page.push_back({m.value(QStringLiteral("id")).toLongLong(), authorId, ss(author.value(QStringLiteral("name")).toString()),
                        ss(m.value(QStringLiteral("body")).toString()), m.value(QStringLiteral("createdAt")).toDouble(),
                        m.value(QStringLiteral("mine")).toBool()});
    }
    // Rows are inserted where they belong so the view keeps its place: a newer page at the end,
    // an older page at the start; anything else (rare) resets the list.
    std::vector<hub::ChatMessage> fresh;
    for (const hub::ChatMessage& m : page)
        if (!log_.contains(m.id))
            fresh.push_back(m);
    if (fresh.empty())
        return;
    std::sort(fresh.begin(), fresh.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    // the same id twice within one page counts once
    fresh.erase(std::unique(fresh.begin(), fresh.end(), [](const auto& a, const auto& b) { return a.id == b.id; }), fresh.end());
    const int before = rowCount();
    const bool atEnd = log_.empty() || fresh.front().id > log_.newestId();
    const bool atStart = !log_.empty() && fresh.back().id < log_.oldestId();
    const int count = static_cast<int>(fresh.size());
    if (atEnd) {
        beginInsertRows(QModelIndex(), before, before + count - 1);
        log_.merge(fresh);
        endInsertRows();
        if (before > 0) // the previous last row may now continue a group
            emit dataChanged(index(before - 1), index(before - 1));
        emit appended(fresh.back().mine);
    } else if (atStart) {
        beginInsertRows(QModelIndex(), 0, count - 1);
        log_.merge(fresh);
        endInsertRows();
        emit dataChanged(index(count), index(count)); // the old first row may no longer start a group
        if (firstUnreadRow_ >= 0)
            firstUnreadRow_ += count;
    } else {
        beginResetModel();
        log_.merge(fresh);
        endResetModel();
    }
}

void HubChatModel::poll()
{
    if (teamId_ == 0 || !live_)
        return;
    const int generation = generation_;
    hub_->request(QStringLiteral("GET"),
                  QStringLiteral("/api/hub/teams/%1/messages?after=%2&wait=25").arg(teamId_).arg(log_.newestId()), QVariant(),
                  [this, generation](int status, const QVariant& data) {
                      if (generation != generation_)
                          return;
                      if (status == 200) {
                          retryDelayMs_ = 2000;
                          merge(data.toMap().value(QStringLiteral("messages")).toList());
                          markRead();
                          QTimer::singleShot(0, this, [this, generation] { if (generation == generation_) poll(); });
                          return;
                      }
                      if (status == 401 || status == 403 || status == 404) {
                          fail(status, data.toMap());
                          return;
                      }
                      // offline, or the server is waking up: try again, waiting longer each time
                      QTimer::singleShot(retryDelayMs_, this, [this, generation] { if (generation == generation_) poll(); });
                      retryDelayMs_ = std::min(retryDelayMs_ * 2, 30000);
                  });
}

void HubChatModel::markRead()
{
    if (!active_ || teamId_ == 0 || log_.newestId() <= markedRead_)
        return;
    markedRead_ = log_.newestId();
    hub_->request(QStringLiteral("POST"), QStringLiteral("/api/hub/teams/%1/read").arg(teamId_),
                  QVariantMap{{QStringLiteral("lastId"), markedRead_}}, [this](int status, const QVariant&) {
                      if (status == 200)
                          hub_->refresh();
                  });
}

void HubChatModel::loadOlder()
{
    if (teamId_ == 0 || loadingOlder_ || !log_.hasOlder())
        return;
    loadingOlder_ = true;
    emit stateChanged();
    const int generation = generation_;
    hub_->request(QStringLiteral("GET"),
                  QStringLiteral("/api/hub/teams/%1/messages?before=%2").arg(teamId_).arg(log_.oldestId()), QVariant(),
                  [this, generation](int status, const QVariant& data) {
                      if (generation != generation_)
                          return;
                      loadingOlder_ = false;
                      if (status == 200) {
                          merge(data.toMap().value(QStringLiteral("messages")).toList());
                          log_.setHasOlder(data.toMap().value(QStringLiteral("hasMore")).toBool());
                      } else {
                          error_ = data.toMap().value(QStringLiteral("error")).toString();
                      }
                      emit stateChanged();
                  });
}

QString HubChatModel::send(const QString& text)
{
    if (teamId_ == 0)
        return tr("Open a team's chat first.");
    QString cleaned;
    try {
        cleaned = qs(hub::cleanText(ss(text), hub::kMaxTeamMessage, "Message", true));
    } catch (const teamforge::TeamForgeError& error) {
        return qs(error.what()) + QLatin1Char('.');
    }
    sending_ = true;
    error_.clear();
    emit stateChanged();
    const int generation = generation_;
    hub_->request(QStringLiteral("POST"), QStringLiteral("/api/hub/teams/%1/messages").arg(teamId_),
                  QVariantMap{{QStringLiteral("body"), cleaned}}, [this, generation](int status, const QVariant& data) {
                      if (generation != generation_)
                          return;
                      sending_ = false;
                      if (status == 201) {
                          merge({data.toMap().value(QStringLiteral("message"))});
                          markedRead_ = std::max(markedRead_, log_.newestId()); // the server marks one's own message read
                      } else {
                          error_ = data.toMap().value(QStringLiteral("error")).toString();
                      }
                      emit stateChanged();
                  });
    return {};
}
