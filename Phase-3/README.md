# Phase 3

Phase 3 development files and submission materials will be added here.

## Feature 1: 3D motivational quotes

Context-aware, ten-word motivational quotes drawn with depth inside the existing forms. The code
lives in the Phase-2 application tree (it is the same app); this folder documents it.

- **Quote library:** [`motivational-quotes.html`](motivational-quotes.html) shows all 66 quotes in
  their category colours. It is generated from `Phase-2/src/core/Motivation.cpp`, so the page and
  the app always match.

### Where quotes appear

| Place | Moment | Categories |
|---|---|---|
| Onboarding step 01 (name, programme) | `register` | Growth |
| Onboarding step 02, right under the selected skill tags | `skills` | depends on the skills (below) |
| Onboarding step 03, after levels and learning interests | `skills` | depends on the skills and levels |
| Opportunity drawer footer, directly above **Express interest** | `join` | Collaboration, Teamwork |
| Host → Projects → new project form | `host_project` | Leadership, Build, Innovation |

Onboarding's Back / Continue / Finish row is now pinned in a bar under the steps, so a quote (or a
long step) can never push it out of view (checked at 1280×720). The quote reacts to every skill
added or removed and every level changed, without touching the form's state.

The opportunity drawer now follows the joining journey: project and fit, what the team needs,
a **team preview** (your avatar, the key skills you bring, members of saved teams for the project),
how joining works, then a footer that is always in view with the pre-joining quote and the
existing **Express interest** button. The button is never hidden behind an animation.

### Skill rules (deterministic, no AI call)

Applied in order by `MotivationLibrary::categoriesFor` on the skills and levels the participant
entered themselves:

1. No skills → First steps.
2. At least half the skills at level 4+ (Advanced / Expert) → Experience. Adding skills at the
   default level never reaches this rule, so nobody is called an expert for entering a skill.
3. Every skill at level 1–2 → Keep learning.
4. One skill → Focus; two or more → Versatility. Learning interests add Keep learning to the pool.

Within one showing of a form the quote stays the same while the rule's result stays the same, so
typing never reshuffles it. It changes when the result changes (e.g. the second skill is added)
and on the next visit. The last six quotes shown sit in a bounded FIFO queue and are skipped while
another quote is available. Quotes never touch matching, ranking or scores.

### Course concepts

- **TCS-302:** hash map from category to quote indices (`unordered_map`); bounded FIFO queue
  of recently shown quotes (`deque`); rotation with a seeded linear congruential generator.
- **TCS-307:** `MotivationLibrary` class, scoped enums, exceptions (`ValidationError` for a quote
  that is not ten words, has no single highlight word, or repeats another), C++ / QML bridge.

### Visuals (`Phase-2/ui/components/MotivationQuote.qml`)

Plain Qt Quick (no Qt Quick 3D, no extra modules): every word is extruded from stacked copies and
flips up into place one after another; the key word carries the category's gradient and a soft
glow; behind the text are two drifting colour fields, a dot texture, rising particles and a glass
sheen, and the card tilts towards the pointer. Colours come from `ui/Theme.qml` and keep their
meanings (blue = the participant's skills, amber = experience, teal = working together,
red = hosts and building, violet = new ideas, green = growth).

- **Reduced motion:** Windows *Animation effects* off, or the browser's `prefers-reduced-motion`,
  keeps the look but stops every animation (`Backend.reduceMotion`).
- **Phones and tablets:** in the browser build a coarse pointer turns off the glow, gradient,
  texture, particles and tilt (`Backend.lightVisuals`); very narrow cards do the same.
- Overrides for testing: `TEAMFORGE_REDUCE_MOTION=1`, `TEAMFORGE_LIGHT_VISUALS=1`.

### Files

| File | Change |
|---|---|
| `Phase-2/src/core/Motivation.h`, `.cpp` | new: quote library, rules, selection |
| `Phase-2/src/app/TeamForgeMotivation.cpp` | new: `Backend.motivation`, `newMotivationVisit`, `motivationLibrary`, motion preferences |
| `Phase-2/src/app/TeamForgeController.h` | the declarations above, `reduceMotion`, `lightVisuals` |
| `Phase-2/ui/components/MotivationQuote.qml` | new: the 3D quote card |
| `Phase-2/ui/Theme.qml` | `quoteStyles` / `quoteStyle()` |
| `Phase-2/ui/participant/ProfileEditor.qml` | quotes in steps 01–03; navigation row pinned to the bottom |
| `Phase-2/ui/participant/OpportunityDrawer.qml` | team preview, footer with quote and Express interest |
| `Phase-2/ui/host/ProjectsPage.qml` | quote on the new-project form |
| `Phase-2/tests/tst_motivation.cpp` | new: library, rules, no-repeat tests |
| `Phase-2/CMakeLists.txt`, `src/core/CMakeLists.txt`, `tests/CMakeLists.txt` | the new files |

No database or data-file changes, no new dependencies or Qt modules.

### Not part of this feature

GetLayers AI was not used: its MCP server was not connected in the session that built this, so
the effects were recreated in Qt Quick. Avatars with outfits, profile pictures, the resume
analyser, AI matching changes and the Azure migration are separate Phase 3 items.

## Feature 2: resume analyser

Upload a PDF or DOCX in onboarding step 01 (or when editing the profile). TeamForge reads it,
then a review drawer shows what it found, each value with the resume line it came from. The person
corrects anything, picks what to add, and only then saves the profile.

- **Reading the file** (`core/Inflate.*`, `core/DocumentText.*`): standard C++ with no new
  libraries. A DEFLATE decoder (canonical Huffman codes) handles ZIP/DOCX and PDF FlateDecode. The
  DOCX reader parses WordprocessingML: header parts, tabs, entities, and text boxes without Word's
  duplicate copy. The PDF reader handles objects, object streams, page trees, Type0/CID and simple
  fonts, ToUnicode maps, glyph widths, Tj/TJ/Td/Tm and form XObjects. It was checked on Chrome/Edge
  PDFs and on the pdfTeX/beamer PDFs in this repository (about 100 ms each).
- **Clear errors** for unsupported or old .doc files, damaged or encrypted files, scanned images
  with no text, and files over the limit (`TEAMFORGE_RESUME_MAX_MB`, default 20).
- **Analysis** (`core/ResumeAnalyzer.*`, abstract `ResumeExtractor` → `RuleBasedResumeExtractor`):
  - Finds the name, an age only when it is written out, and the skills TeamForge knows, matched by a
    word trie over the catalogue, profiles and projects. Well-known spellings such as "ReactJS" and
    "k8s" count only if their target skill exists.
  - Skills that are everyday words ("go", "r", "testing") count only under a skills heading.
  - Skills listed but unknown to TeamForge are offered for review and never added on their own.
  - Also extracts per-skill years, total experience, projects, education, certifications and
    achievements.
  - Deterministic: no AI call, no API key. Another provider can implement `ResumeExtractor` later.
- **Years → level**: 0–1 Beginner, >1–2 Basic, >2–4 Intermediate, >4–5 Advanced, >5 Expert. Years
  come from the skill's own statement ("Python (4 years)"; a statement never crosses a comma), or
  else from the dated roles that mention the skill. Overlapping roles count once (sort + merge
  intervals). Total experience is never copied onto skills. A skill with no stated experience gets
  no suggested level: the person chooses one or unticks it.
- **Privacy**: the file is held in memory during review and is never written to the shared data
  files. Only confirmed details are saved, as optional `projects`, `education`, `certifications`,
  `achievements`, `skillYears` and `totalYears` fields on the student record. Records without them
  are unchanged, so the sample data is unchanged too. Desktop users can choose to keep a private
  local copy (`<data>/private/`, never synced) and delete it from My profile. The browser build
  never keeps the file. The log records only file type, size and counts. Age is shown but never
  stored or used.
- **Matching is unchanged**: profile details are display only.

Tests: `tst_resume` (20 cases, with fixtures in `tests/data/resumes/`), plus new `tst_controller`
and `tst_ui` cases for the whole import → review → save flow.
Not verified: the WebAssembly build (browser file picker via `EM_JS`/embind, and the static
`QtQuick.Dialogs` import). Run the GitHub web workflow and test on the live site before relying on it.
