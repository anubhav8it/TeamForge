#pragma once

#include "core/Hub.h"

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QImage>
#include <QJSValue>
#include <QSet>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <functional>

#ifndef Q_OS_WASM
class QNetworkAccessManager;
#endif

class HubClient;

// One team's chat for the QML chat view: the messages of core::hub::ChatLog as list rows, the
// newest page first, older pages on request, and new messages as they arrive (long polling:
// the server holds each request until a message comes or 25 seconds pass, then the next one
// starts). Opening another team, or close(), drops the old conversation; late replies to it
// are ignored.
class HubChatModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ANONYMOUS

    Q_PROPERTY(int teamId READ teamId NOTIFY stateChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY stateChanged)
    Q_PROPERTY(bool loadingOlder READ loadingOlder NOTIFY stateChanged)
    Q_PROPERTY(bool hasOlder READ hasOlder NOTIFY stateChanged)
    Q_PROPERTY(bool sending READ sending NOTIFY stateChanged)
    Q_PROPERTY(bool live READ live NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    // Row of the first unread message when the chat was opened, or -1 ("New messages" divider).
    Q_PROPERTY(int firstUnreadRow READ firstUnreadRow NOTIFY stateChanged)
    // While false (the chat is not on screen), new messages are not marked as read.
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY stateChanged)

public:
    enum Role {
        MessageIdRole = Qt::UserRole + 1, BodyRole, AuthorIdRole, AuthorNameRole, AuthorColorRole, AuthorAvatarRole,
        MineRole, TimeTextRole, DayLabelRole, StartsGroupRole, StartsDayRole, CreatedAtRole
    };

    explicit HubChatModel(HubClient *hub);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int teamId() const { return teamId_; }
    bool loading() const { return loading_; }
    bool loadingOlder() const { return loadingOlder_; }
    bool hasOlder() const { return log_.hasOlder(); }
    bool sending() const { return sending_; }
    bool live() const { return live_; }
    QString error() const { return error_; }
    int firstUnreadRow() const { return firstUnreadRow_; }
    bool active() const { return active_; }
    void setActive(bool active);

    Q_INVOKABLE void open(int teamId);
    Q_INVOKABLE void close();
    Q_INVOKABLE void loadOlder();
    // Returns "" when sent (or on its way), else why the text cannot be sent.
    Q_INVOKABLE QString send(const QString& text);

    // For tests: merge messages as the server would send them.
    void mergeForTest(const QVariantList& messages) { merge(messages); }

signals:
    void stateChanged();
    // New messages at the end (a live update or one's own message): the view may scroll down.
    void appended(bool mine);

private:
    void merge(const QVariantList& messages);
    void poll();
    void markRead();
    void fail(int status, const QVariantMap& data);

    HubClient *hub_;
    teamforge::hub::ChatLog log_;
    QHash<long long, QVariantMap> authors_; // author id -> {color, avatar}
    int teamId_ = 0;
    int generation_ = 0;
    long long lastReadId_ = 0;
    long long markedRead_ = 0;
    int firstUnreadRow_ = -1;
    int retryDelayMs_ = 2000;
    bool loading_ = false;
    bool loadingOlder_ = false;
    bool sending_ = false;
    bool live_ = false;
    bool active_ = false;
    QString error_;
};

// The TeamForge Hub for QML (singleton `Hub`): teams and their private chat, applications to
// teams and events, anonymous connection requests and notifications, all kept by the TeamForge
// server (web/hub.py), which authorises every request against the signed-in account.
//
// Transport: in the browser the page's TF_hub() (web/static/app/app.js) makes the request with
// the page's own sign-in cookie, as TF_readDocument does for pictures; the desktop app uses Qt
// Network with its own sign-in (signIn) against a server address (TEAMFORGE_SERVER or the one
// typed in). Pages call get()/post() with a callback (status, data); status 0 means the server
// could not be reached, and errors come as {error: "..."}.
//
// The rules shown in the UI (who may withdraw, which decisions a reviewer has, why applying is
// unavailable, text limits) come from core/Hub.h, the same rules the server enforces.
class HubClient : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Hub)
    QML_SINGLETON

    Q_PROPERTY(bool available READ available NOTIFY stateChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY stateChanged)
    Q_PROPERTY(bool needsSignIn READ needsSignIn NOTIFY stateChanged)
    Q_PROPERTY(bool browser READ browser CONSTANT)
    Q_PROPERTY(QString serverUrl READ serverUrl NOTIFY stateChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap summary READ summary NOTIFY summaryChanged)
    Q_PROPERTY(QVariantMap profile READ profile NOTIFY summaryChanged)
    Q_PROPERTY(bool needsProfile READ needsProfile NOTIFY summaryChanged)
    Q_PROPERTY(int unreadNotifications READ unreadNotifications NOTIFY summaryChanged)
    Q_PROPERTY(int unreadMessages READ unreadMessages NOTIFY summaryChanged)
    Q_PROPERTY(int pendingDecisions READ pendingDecisions NOTIFY summaryChanged)
    Q_PROPERTY(int anonymousInbox READ anonymousInbox NOTIFY summaryChanged)
    Q_PROPERTY(QVariantList recentNotifications READ recentNotifications NOTIFY notificationsChanged)
    // Where the Community page is: "home", "messages/5", "teams/3", "events/req-001",
    // "applications", "anonymous/<id>", "notifications", "profile", "people".
    Q_PROPERTY(QString route READ route WRITE setRoute NOTIFY routeChanged)
    Q_PROPERTY(HubChatModel *chat READ chat CONSTANT)
    Q_PROPERTY(QString anonymityNotice READ anonymityNotice CONSTANT)
    // Bumped whenever identities or cached photos change, so avatars re-read them.
    Q_PROPERTY(int identityRevision READ identityRevision NOTIFY identitiesChanged)
    Q_PROPERTY(int photoRevision READ photoRevision NOTIFY photosChanged)

public:
    explicit HubClient(QObject *parent = nullptr);
    ~HubClient() override;

    bool available() const;
    bool connected() const { return connected_; }
    bool needsSignIn() const { return !browser() && !connected_; }
    static bool browser();
    QString serverUrl() const { return serverUrl_; }
    QString statusText() const { return statusText_; }
    QVariantMap summary() const { return summary_; }
    QVariantMap profile() const { return summary_.value(QStringLiteral("profile")).toMap(); }
    bool needsProfile() const { return summary_.value(QStringLiteral("needsSetup")).toBool(); }
    int unreadNotifications() const { return summary_.value(QStringLiteral("unreadNotifications")).toInt(); }
    int unreadMessages() const { return summary_.value(QStringLiteral("unreadMessages")).toInt(); }
    int pendingDecisions() const;
    int anonymousInbox() const { return summary_.value(QStringLiteral("anonymousInbox")).toInt(); }
    QVariantList recentNotifications() const;
    QString route() const { return route_; }
    void setRoute(const QString& route);
    HubChatModel *chat() const { return chat_; }
    static QString anonymityNotice();
    int identityRevision() const { return identityRevision_; }
    int photoRevision() const { return photoRevision_; }

    // ---- one identity per person
    // The account identity ({id, name, avatar, color, studentId}) using participant profile
    // `studentId`, or {} (a sample record, or not connected). Every avatar asks this, so the same
    // person looks the same everywhere.
    Q_INVOKABLE QVariantMap identityForStudent(const QString& studentId) const;
    // The picture for a photo reference ("/api/hub/photos/7?v=2") as a data: URL, or "" while it
    // is fetched (photoRevision then changes). data: URLs are returned unchanged.
    Q_INVOKABLE QString photo(const QString& ref);
    // After the participant profile is saved: the account's name and academic details follow it,
    // and the account is linked to it.
    Q_INVOKABLE void syncIdentity(const QString& name, const QString& program, const QString& studentId);
    Q_INVOKABLE void uploadPhoto(const QString& dataUrl, const QJSValue& callback = QJSValue());
    Q_INVOKABLE void removePhoto(const QJSValue& callback = QJSValue());
    // Crop the picture last picked (avatarSourceReady): a square of `side` source pixels at
    // (x, y), scaled to a small JPEG. Returns a data: URL, or "" (avatarFailed explains).
    Q_INVOKABLE QString cropAvatar(double x, double y, double side);

    // ---- requests (paths after /api/hub)
    // A full address for a server path (a team invitation link): the page's own address in the
    // browser, the server the desktop app signed in to otherwise.
    Q_INVOKABLE QString absoluteUrl(const QString& path) const;
    // Copies text to the clipboard (browser: the page's clipboard; desktop: the system's).
    Q_INVOKABLE bool copyText(const QString& text);
    Q_INVOKABLE void get(const QString& path, const QJSValue& callback = QJSValue());
    Q_INVOKABLE void post(const QString& path, const QVariant& body, const QJSValue& callback = QJSValue());
    // Desktop: sign in to a TeamForge server with an account made on its web page.
    Q_INVOKABLE void signIn(const QString& server, const QString& email, const QString& password,
                            const QJSValue& callback = QJSValue());
    Q_INVOKABLE void signOut();
    // Summary (counts, own profile) and the latest notifications; also runs every 30 seconds.
    Q_INVOKABLE void refresh();
    // A notification's link ("#/teams/5") -> route; marks it read. False for links outside the hub.
    Q_INVOKABLE bool openNotification(const QVariantMap& notification);
    Q_INVOKABLE void markAllNotificationsRead();

    // ---- rules and wording from core/Hub.h
    Q_INVOKABLE QString statusLabel(const QString& status) const;
    // Chip tone for a status: success, warning, info, accent, muted.
    Q_INVOKABLE QString statusTone(const QString& status) const;
    // [{value: "approve" | "waitlist" | "reject", text, status}] for a reviewer.
    Q_INVOKABLE QVariantList reviewerActions(const QString& kind, const QString& status) const;
    Q_INVOKABLE bool canWithdraw(const QString& kind, const QString& status, bool deadlinePassed = false) const;
    // Why the team's "Apply" is unavailable (from Hub teams/<id>), or "".
    Q_INVOKABLE QString teamApplyBlocker(const QVariantMap& team) const;
    Q_INVOKABLE bool canRemoveMember(const QString& actorRole, const QString& targetRole) const;
    Q_INVOKABLE bool canChangeRoles(const QString& role) const;
    Q_INVOKABLE bool canManageTeam(const QString& role) const;
    // "" when the text is acceptable, else the reason (limits as on the server).
    Q_INVOKABLE QString checkText(const QString& text, int limit, const QString& what, bool required = false) const;
    Q_INVOKABLE int characterCount(const QString& text) const;
    Q_INVOKABLE QString relativeTime(double epochSeconds) const;
    Q_INVOKABLE QString dateText(double epochSeconds) const;
    Q_INVOKABLE QString deadlineText(double epochSeconds) const;
    Q_INVOKABLE double now() const;
    // "2026-12-01" (local midnight at the end of that day) -> seconds since the epoch, or 0.
    Q_INVOKABLE double parseDate(const QString& text) const;
    Q_INVOKABLE QString isoDate(double epochSeconds) const;

    // ---- profile picture
    // Opens the browser's or the system's file picker; the result arrives as avatarPicked
    // (a small JPEG/PNG data: URL) or avatarFailed.
    Q_INVOKABLE void pickAvatar();
    Q_INVOKABLE QString avatarFromFile(const QUrl& file);
    void avatarBytes(const QByteArray& bytes);

    // ---- transport, used by HubChatModel and the QML functions above
    using Callback = std::function<void(int status, const QVariant& data)>;
    void request(const QString& method, const QString& path, const QVariant& body, Callback done);
    // Browser replies come back here (web build: TF_hub -> tfHubReply).
    void deliver(int requestId, int status, const QByteArray& body);

    // For tests: the callback-free summary update, as if the server had answered.
    void setSummaryForTest(const QVariantMap& summary) { applySummary(summary); }

signals:
    void stateChanged();
    void summaryChanged();
    void notificationsChanged();
    void routeChanged();
    void avatarPicked(const QString& dataUrl);
    // A picture was chosen: crop it (cropAvatar) using this preview of `width` x `height`.
    void avatarSourceReady(const QString& preview, int width, int height);
    void avatarFailed(const QString& message);
    void identitiesChanged();
    void photosChanged();
    // Every successful write, so open pages can reload what it changed.
    void changed(const QString& path);

private:
    void callJs(const QJSValue& callback, int status, const QVariant& data);
    void applySummary(const QVariantMap& summary);
    void loadIdentities();
    void handleStatus(int status);
    QString encodeAvatar(const QByteArray& bytes, QString *error) const;

    HubChatModel *chat_ = nullptr;
    QTimer refreshTimer_;
    QHash<int, Callback> pending_;
    int nextRequest_ = 1;
    bool connected_ = false;
    QString serverUrl_;
    QString statusText_;
    QVariantMap summary_;
    teamforge::hub::NotificationList notifications_;
    QHash<QString, QVariantMap> identities_; // participant profile id -> account identity
    QString identityVersion_;
    int identityRevision_ = 0;
    QHash<QString, QString> photos_;          // photo reference -> data: URL
    QSet<QString> photosPending_;
    int photoRevision_ = 0;
    QImage avatarSource_;
    QString route_ = QStringLiteral("home");
#ifndef Q_OS_WASM
    QNetworkAccessManager *network_ = nullptr;
#endif
};
