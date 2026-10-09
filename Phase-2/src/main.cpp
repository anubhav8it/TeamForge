#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QtQml/QQmlExtensionPlugin>

#ifdef Q_OS_WASM
#include "WebBridge.h" // browser build only: data comes from and goes to the TeamForge server
#endif

// The TeamForge QML module (UI and bridge) is linked in statically from teamforge_ui.
Q_IMPORT_QML_PLUGIN(TeamForgePlugin)

namespace {

// Manrope (SIL Open Font License, assets/fonts/OFL.txt), bundled through the QML module's
// RESOURCES. If a file fails to load, Qt falls back to the system UI font.
void loadBundledFonts()
{
    const char *const files[] = {"Manrope-Regular", "Manrope-Medium", "Manrope-SemiBold", "Manrope-Bold",
                                 "Manrope-ExtraBold"};
    for (const char *file : files) {
        const QString path = QStringLiteral(":/qt/qml/TeamForge/assets/fonts/%1.ttf").arg(QLatin1String(file));
        if (QFontDatabase::addApplicationFont(path) < 0)
            qWarning("Could not load bundled font %s", qPrintable(path));
    }
#ifdef Q_OS_WASM
    // A web page has no system fonts; the developer pages' code text uses this one (Theme.monoFamily).
    if (QFontDatabase::addApplicationFont(QStringLiteral(":/qt/qml/TeamForge/assets/fonts/NotoSansMono-Regular.ttf")) < 0)
        qWarning("Could not load the bundled monospace font");
#endif
    QFont font(QStringLiteral("Manrope"));
    font.setHintingPreference(QFont::PreferNoHinting);
    QGuiApplication::setFont(font);
}

} // namespace

int main(int argc, char *argv[])
{
#ifdef Q_OS_WASM
    // Before anything creates the Backend singleton: it reads TEAMFORGE_DATA_DIR and
    // TEAMFORGE_ALLOWED_WORKSPACES, which this sets from the signed-in account.
    webbridge::prepareDataDirectory();
#endif
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("TeamForge");
    QGuiApplication::setApplicationVersion(TEAMFORGE_VERSION);
    // Bundled through the QML module's RESOURCES.
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/qt/qml/TeamForge/assets/teamforge_mark_dark.png")));
    loadBundledFonts();

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.loadFromModule("TeamForge", "Main");
#ifdef Q_OS_WASM
    webbridge::startSync(&app);
#endif

    return app.exec();
}
