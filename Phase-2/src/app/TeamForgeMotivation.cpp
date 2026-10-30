// Motivational quotes and the motion preferences the quote visuals follow. Part of
// TeamForgeController. The rules and the library live in core/Motivation.h; this only adapts them.
#include "TeamForgeController.h"

#include "BridgeSupport.h"

#include <QByteArray>
#include <QStringList>
#include <QtGlobal>

#include <algorithm>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(Q_OS_WASM)
#include <emscripten.h>
#endif

using namespace teamforge;
using namespace bridge;

namespace {

// "1" / "0" in the environment overrides the system; anything else (or unset) does not.
int environmentOverride(const char *name)
{
    const QByteArray value = qgetenv(name);
    return value == "1" ? 1 : value == "0" ? 0 : -1;
}

QVariantMap quoteToMap(const Quote& quote)
{
    const QString text = qs(quote.text);
    return {{QStringLiteral("text"), text},
            {QStringLiteral("highlight"), qs(quote.highlight)},
            {QStringLiteral("highlightIndex"), quote.highlightIndex},
            {QStringLiteral("category"), qs(std::string(categoryId(quote.category)))},
            {QStringLiteral("words"), text.split(QLatin1Char(' '), Qt::SkipEmptyParts)}};
}

} // namespace

bool TeamForgeController::systemReducesMotion()
{
    const int forced = environmentOverride("TEAMFORGE_REDUCE_MOTION");
    if (forced >= 0)
        return forced == 1;
#if defined(Q_OS_WIN)
    // Settings > Accessibility > Visual effects > Animation effects (what browsers map
    // prefers-reduced-motion to on Windows).
    BOOL animations = TRUE;
    if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animations, 0))
        return animations == FALSE;
#elif defined(Q_OS_WASM)
    return emscripten_run_script_int(
               "window.matchMedia && window.matchMedia('(prefers-reduced-motion: reduce)').matches ? 1 : 0")
           != 0;
#endif
    return false;
}

bool TeamForgeController::systemPrefersLightVisuals()
{
    const int forced = environmentOverride("TEAMFORGE_LIGHT_VISUALS");
    if (forced >= 0)
        return forced == 1;
#if defined(Q_OS_WASM)
    // Phones and tablets: a coarse pointer and no hover. They get the light quote visuals.
    return emscripten_run_script_int(
               "window.matchMedia && window.matchMedia('(pointer: coarse)').matches ? 1 : 0")
           != 0;
#else
    return false;
#endif
}

int TeamForgeController::newMotivationVisit()
{
    return ++motivationVisits_;
}

QVariantMap TeamForgeController::motivation(const QString& moment, const QVariantList& skills, int learningCount,
                                            int visit)
{
    QVariantMap result;
    run("motivation", moment, [&] {
        SkillSnapshot snapshot;
        for (const QVariant& entry : skills) {
            const QVariantMap skill = entry.toMap();
            if (skill.value(QStringLiteral("skill")).toString().trimmed().isEmpty())
                continue;
            const int level = skill.value(QStringLiteral("level")).toInt();
            ++snapshot.skillCount;
            snapshot.advancedCount += level >= 4 ? 1 : 0;
            snapshot.maxLevel = std::max(snapshot.maxLevel, level);
        }
        snapshot.learningCount = std::max(0, learningCount);
        result = quoteToMap(motivation_.pick(momentFromId(ss(moment)), snapshot, visit));
    }, false);
    return result;
}

QVariantList TeamForgeController::motivationLibrary() const
{
    QVariantList list;
    for (const Quote& quote : motivation_.quotes())
        list.append(quoteToMap(quote));
    return list;
}
