# Phase 3

Phase 3 development files and submission materials will be added here.

## Feature 1:  quotes

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

## Feature 5: hosting on Azure, with your own domain

The web version moves from Render, whose free database expires after 30 days, to Azure. On Azure
for Students it fits the free allowances: Container Apps (scales to zero) and PostgreSQL Flexible
Server B1ms (free for 12 months). A custom domain gets a free managed HTTPS certificate. The app
and server code are unchanged; the server already used PostgreSQL through `DATABASE_URL`.

- `Phase-2/Dockerfile` and `.dockerignore`: an image of only `web/` and `data/`, on Python 3.12
  slim, run as an unprivileged user. The app files are gzipped at build time, so cold starts are
  quick.
- `.github/workflows/deploy-azure.yml` runs after **Build web app** succeeds on main:
  - builds the image and pushes it to ghcr.io (free);
  - if an `AZURE_CREDENTIALS` secret is set, rolls it out with `az containerapp update` and
    waits for `/healthz`;
  - otherwise prints the one Cloud Shell command to deploy it.
- `Phase-2/web/tools/copy_database.py` copies accounts (with their password hashes), sessions,
  shared data and each account's session file from Render's PostgreSQL to Azure's:
  - it reads the URLs from environment variables, so no passwords end up in shell history;
  - it refuses a target that already has data unless given `--replace`;
  - it checks the row counts and continues the account ids;
  - it prints counts only.
- `Phase-2/web/AZURE-DEPLOY.md`: every portal step, with the exact free sizes, the order that
  avoids losing data, the domain's DNS records, a budget alert, and when to delete Render.

Tests: `web/tests/test_copy_database.py` covers SQLite to SQLite:
- passwords still work and sessions survive;
- new ids follow the copied ones;
- a non-empty target is refused, and `--replace` works;
- no password is printed.

The server was also run from a folder holding only the image's contents (health check, sign-in
page, gzipped app). Docker itself was not run locally (no Docker here). Nothing in Azure is
created by this repository; you create the resources yourself, following the guide.

## Feature 4: any kind of resume, including photos and scans

The resume import now takes almost any file a student has, not only PDF and DOCX:

| File | Read by |
|---|---|
| PDF with text, DOCX | the app (as before) |
| ODT (LibreOffice / Google Docs export), RTF, TXT, Markdown, HTML | the app: new readers in `core/DocumentText.*` |
| Photo of a printed resume (JPEG, PNG, BMP, GIF; in the browser also HEIC, WebP, AVIF) | Azure AI Document Intelligence, through the server, after the person agrees |
| PDF of scanned pages (no text inside) | the same, sent as it is |
| Old `.doc`, PowerPoint, Excel, ZIP folders, programs | refused with what to do instead |

**Blurry or unreadable pictures are refused**, in two steps:

1. *Before anything is sent* (`core/ScanQuality.*`, standard C++): the app decodes the photo,
   turns it upright, makes it grey and scales it to 1600 px. It then refuses it if it is too small
   (under 500 × 700 px), too dark, blank, too blurry or too faint, each with its own message. Blur
   is measured per 24 px tile that has detail. The score is the RMS Laplacian divided by the
   tile's standard deviation, which ignores contrast; the median over those tiles is used. The
   limit (0.35) was calibrated on rendered resume pages blurred by known amounts. Sharp print
   scores 1.5–2.5, and letters start to merge (blur sigma 1.6 px) at about 0.28. It is a
   heuristic and errs on letting a picture through, because of step 2.
2. *After reading* (`acceptReadText`): Azure reports how sure it is of each word. If the average
   is under 0.75, or more than a quarter of the words are under 0.6, the text is not trusted and
   the person is asked for a sharper photo. Too little text is refused too.

**Consent and privacy.** A photo or scan is never sent without asking. The card says "Read the text
from this photo?" and names Microsoft Azure; nothing leaves until **Read the text** is clicked.
Photos are re-encoded as JPEG (at most 3000 px, at most 4 MB), which also drops the camera's
metadata. The server checks sign-in, the TeamForge header and same origin, the size, the file
type and per-account and per-server rate limits. It deletes Azure's copy of the result at once
and stores and logs neither the picture nor the text. The rest is as in Feature 2: only reviewed
details are saved.

**Without Azure** (the desktop app, or a server without `AZURE_DOCINTEL_*` set) photos are still
checked for blur, then the person is told where pictures can be read and what to upload instead.
Setup: `Phase-2/web/README-WEB.md`, "Optional: read photos and scans". It needs a free (F0)
Document Intelligence resource that you create yourself; this repository creates nothing in Azure.

Tests:
- `tst_resume`: ODT, RTF, HTML, Markdown and UTF-16 text; types and refusals; blur, darkness,
  faintness, blank and size checks; OCR confidence.
- `tst_controller`: the whole flow with a stand-in reader. It covers the consent step, a blurry
  photo never being sent, the server's errors, a late answer after Cancel, the scanned-PDF size
  limit and a log free of names.
- `tst_ui`: the card asks, sends, and the review opens.
- `web/tests/test_server.py`: a fake Azure service. It checks sign-in and same-origin rules,
  that the request goes to prebuilt-read v4.0, that the result is deleted after reading, that a
  result URL on another host is not followed, the rate limit, the 4 MB limit, the 503 without
  settings, and a log free of key, picture and text.

The test harness now reads the server's log as it is written. A full pipe used to stall the
server, which was the "reset restores seed" timeout on slower machines.

Not verified here: the WebAssembly build (`EM_JS`/embind glue and the browser's image conversion)
and a real Azure call. Check both on the live site after setting the Azure variables.

## Feature 3: a note with your interest

When a participant expresses interest (in the opportunity drawer), they can add a short note for
the host, for example the part they'd like to work on, or experience their skills list doesn't
show. The host sees the note under the applicant on the Applicants page and can **Reply** (or
**Edit reply**) at any stage; the participant sees the reply in the drawer's Messages section, and
a "Host replied" chip on their home page.

- Notes and replies are plain text, at most 500 characters, trimmed, control characters removed.
- The participant can edit or add their note only while the request is still new. Once the host
  starts reviewing it, it stays as the host read it.
- Stored on the request in `interest_requests.json` (`note`, `reply`); requests without messages
  are stored exactly as before, so the sample data is unchanged.
- Permissions are checked in C++: only the participant writes their note, only a host replies.
  The log records that a note or reply was sent, never what it says.
- The web server needs no change, but it trusts the app's copy of `interest_requests.json` (as it
  already does for interest statuses). Server-side checks are worth adding with the Azure move.

Tests: `tst_domain` (rules and limits), `tst_persistence` (stored format), `tst_controller`
(permissions, locking, reload), `tst_ui` (note sent from the drawer reaches the host's list).

