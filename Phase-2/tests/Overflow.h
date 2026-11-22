#pragma once

#include <QQuickItem>
#include <QQuickWindow>
#include <QStringList>

#include <algorithm>
#include <functional>

// Phone layouts: content that sticks out past the right edge of the window (horizontal overflow),
// i.e. text, controls or pictures that would be cut off. Skipped: anything inside something that
// scrolls sideways on purpose (a Flickable wider than itself: a scrolling tab row, the desktop
// tools' area), anything hidden by a clipping parent (decorative glows inside a clipped card), and
// pure decoration without text. Returns "Type 'text' right=…" for each offender (outermost only).
inline QStringList horizontalOverflow(QQuickWindow *window, double tolerance = 1.0)
{
    QStringList found;
    const double limit = window->width() + tolerance;
    const std::function<void(QQuickItem *, double)> visit = [&](QQuickItem *item, double clipRight) {
        if (!item->isVisible() || item->opacity() <= 0.0 || item->width() <= 0)
            return;
        if (item->inherits("QQuickFlickable") && item->property("contentWidth").toReal() > item->width() + tolerance)
            return; // scrolls sideways on purpose
        const double right = item->mapToScene(QPointF(item->width(), 0)).x();
        const double shownRight = std::min(right, clipRight);
        const QString text = item->property("text").toString();
        const bool content = !text.isEmpty() || item->inherits("QQuickTextInput") || item->inherits("QQuickTextEdit")
                          || item->inherits("QQuickImage") || item->inherits("QQuickAbstractButton");
        if (content && shownRight > limit) {
            found << QStringLiteral("%1 '%2' right=%3").arg(QString::fromLatin1(item->metaObject()->className()),
                                                           text.left(40)).arg(right, 0, 'f', 0);
            return; // report the outermost item only
        }
        const double childClip = item->clip() ? std::min(clipRight, right) : clipRight;
        for (QQuickItem *child : item->childItems())
            visit(child, childClip);
    };
    visit(window->contentItem(), window->width() * 10.0);
    return found;
}
