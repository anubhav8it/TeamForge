pragma Singleton

import QtQuick

// Design tokens for the whole UI. Colours, type and spacing live here only.
// Colour carries meaning: red = primary action and coverage, green = met / covered / accepted,
// blue = engagement, interest and information, amber = experience and warnings,
// teal = complementarity, violet = diversity and the developer workspace.
QtObject {
    // Bright graphite surfaces, darkest to lightest: elevated cards read clearly against the page.
    readonly property color sidebar: "#1A1E25"
    readonly property color background: "#20252D"
    readonly property color surface: "#2A303A"
    readonly property color surfaceRaised: "#343B47"
    readonly property color surfaceHover: "#3D4552"
    readonly property color input: "#1E232B"
    readonly property color border: "#3A424F"
    readonly property color borderStrong: "#4E5868"

    readonly property color text: "#F8F9FB"
    readonly property color textSecondary: "#D2D7DF"
    readonly property color textMuted: "#A3ACB8"

    readonly property color accent: "#F0434A"
    readonly property color accentHover: "#FF5C62"
    readonly property color accentPressed: "#D2353C"
    readonly property color accentSubtle: "#2EF0434A"
    readonly property color selection: "#26F0434A"

    readonly property color success: "#3DD598"
    readonly property color successSubtle: "#263DD598"
    readonly property color info: "#5AB0FF"
    readonly property color infoSubtle: "#265AB0FF"
    readonly property color warning: "#FFB547"
    readonly property color warningSubtle: "#26FFB547"
    readonly property color teal: "#2DD4BF"
    readonly property color violet: "#A78BFA"

    // Score factors in factorDefinitions order.
    readonly property var factorColors: [accent, teal, warning, info, violet]

    // Avatar gradients: hand-picked pairs, all dark enough for white initials (see avatarStyle()).
    readonly property var avatarGradients: [
        ["#FF6B6B", "#C2255C"], ["#FF922B", "#E8590C"], ["#F59F00", "#D9480F"], ["#51CF66", "#2B8A3E"],
        ["#20C997", "#087F5B"], ["#22B8CF", "#0B7285"], ["#4DABF7", "#1864AB"], ["#748FFC", "#3B5BDB"],
        ["#9775FA", "#6741D9"], ["#DA77F2", "#9C36B5"], ["#F783AC", "#C2255C"], ["#FF8787", "#E8590C"],
        ["#38D9A9", "#1971C2"], ["#B197FC", "#E64980"]
    ]

    // Typography: Manrope, bundled with the app (src/main.cpp registers it).
    readonly property string displayFamily: "Manrope"
    readonly property string fontFamily: "Manrope"
    // Code and raw-record text. Consolas ships with Windows; the browser build bundles
    // Noto Sans Mono (SIL Open Font License) because a web page has no system fonts.
    readonly property string monoFamily: Qt.platform.os === "wasm" ? "Noto Sans Mono" : "Consolas"
    // Minimum 13 px; primary information starts at 16 px.
    readonly property int fontXs: 13
    readonly property int fontSm: 15
    readonly property int fontBase: 16
    readonly property int fontMd: 17
    readonly property int fontLg: 21
    readonly property int fontXl: 31
    readonly property int fontScore: 29
    readonly property int fontDisplay: 40

    readonly property int radius: 12
    readonly property int radiusSmall: 8
    readonly property int gap: 16
    readonly property int pad: 20
    readonly property int animFast: 120
    readonly property int animNormal: 200
    readonly property int animSlow: 420

    readonly property url logo: Qt.resolvedUrl("../assets/teamforge_logo_dark.png")
    readonly property url mark: Qt.resolvedUrl("../assets/teamforge_mark_dark.png")
    // Full-resolution layers of the monogram for the entry screen's 3D logo.
    readonly property url markLight: Qt.resolvedUrl("../assets/teamforge_mark_light.png")
    readonly property url markRed: Qt.resolvedUrl("../assets/teamforge_mark_red.png")
    readonly property url markDepth: Qt.resolvedUrl("../assets/teamforge_mark_depth.png")

    function percent(value: real): string {
        return Math.round(value * 100) + "%"
    }

    // "3 members" / "1 member".
    function count(n: int, singular: string, plural: string): string {
        return n + " " + (n === 1 ? singular : plural)
    }

    // Skills are displayed in capitals; the backend keeps its normalised lower-case names.
    function skill(name: string): string {
        return name.toUpperCase()
    }

    // Plain-language band for an average skill level (1-5).
    function experienceLabel(level: real): string {
        return level >= 4 ? qsTr("Advanced") : level >= 3 ? qsTr("Intermediate")
             : level >= 2 ? qsTr("Developing") : qsTr("Beginner")
    }

    function initials(name: string): string {
        const parts = name.trim().split(/\s+/).filter(part => /^[A-Za-zÀ-ɏ]/.test(part))
        const first = parts.length > 0 ? parts[0].charAt(0) : ""
        const last = parts.length > 1 ? parts[parts.length - 1].charAt(0) : ""
        return String(first + last).toUpperCase()
    }

    // djb2 over the seed, as an unsigned 32-bit value (typed real: an int would wrap negative).
    function hash(seed: string): real {
        let h = 5381
        for (let i = 0; i < seed.length; ++i)
            h = ((h * 33) ^ seed.charCodeAt(i)) >>> 0
        return h
    }

    // Same seed (participant id) -> same gradient and angle, on every run and machine.
    function avatarStyle(seed: string): var {
        const h = hash(seed)
        const pair = avatarGradients[h % avatarGradients.length]
        return { from: pair[0], to: pair[1], angle: (Math.floor(h / avatarGradients.length) % 4) * 45 - 45 }
    }

    function avatarColor(seed: string): color {
        return avatarStyle(seed).from
    }

    // Short fit line for an opportunity card (Backend.opportunities entry): only restates counts
    // the backend computed.
    function fitSummary(opportunity: var): string {
        return qsTr("You meet %1 of %2 key skills").arg(opportunity.coveredCount ?? 0).arg(opportunity.requiredCount ?? 0)
    }

    // Interest workflow: the stored status ("interested", "under_review", ...) in words and colour.
    function interestLabel(status: string): string {
        return status === "under_review" ? qsTr("Under review") : status === "accepted" ? qsTr("Accepted")
             : status === "declined" ? qsTr("Declined") : status === "interested" ? qsTr("Interested") : ""
    }
    function interestTone(status: string): string {
        return status === "accepted" ? "success" : status === "declined" ? "muted"
             : status === "under_review" ? "warning" : "info"
    }

    // Level 1-5 in words, for skill rows.
    function levelName(level: int): string {
        return [qsTr("Not set"), qsTr("Beginner"), qsTr("Basic"), qsTr("Intermediate"), qsTr("Advanced"), qsTr("Expert")][Math.max(0, Math.min(5, level))]
    }

    // The three workspaces' identity colours and names.
    function workspaceColor(name: string): color {
        return name === "participant" ? info : name === "developer" ? violet : accent
    }
    function workspaceName(name: string): string {
        return name === "participant" ? qsTr("Participant") : name === "developer" ? qsTr("Developer") : qsTr("Host")
    }

    // Score bands for quick scanning of ranked lists.
    function scoreColor(score: real): color {
        return score >= 0.7 ? success : score >= 0.5 ? warning : textSecondary
    }

    // Project types get a consistent tag colour; unknown types stay neutral.
    function typeColor(type: string): color {
        const t = type.toLowerCase()
        if (t.includes("hackathon")) return accent
        if (t.includes("ai") || t.includes("ml")) return violet
        if (t.includes("web") || t.includes("product")) return info
        if (t.includes("research")) return teal
        if (t.includes("competition")) return warning
        if (t.includes("pbl") || t.includes("course")) return success
        return textSecondary
    }
}
