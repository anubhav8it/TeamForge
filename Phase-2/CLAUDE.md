# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Current state

TeamForge is a role-based desktop app with exactly **three local workspaces**: Participant, Host and Developer (see "Workspaces"). The pieces:
- `src/core`: the C++ core (domain model, matching, data structures, JSON persistence), implemented and tested.
- `src/app`: the C++ bridge (`TeamForgeController` and helpers), which connects the core to the Qt Quick UI in `ui/`.

The Phase-1 report and slides are the source of truth for scope and design. The canonical copies are in `Phase-1/`; the two PDFs in the repository root are git-ignored duplicates.

## Build, run, test

The toolchain is Qt 6.12.0 for MinGW 64-bit, installed under `D:\QT`, with that install's own MinGW 13.1, CMake and Ninja. Don't use the MSYS2 `ucrt64` g++ that is also on this machine. Its runtime doesn't match Qt's prebuilt libraries. The folder is a git repository (`main` tracks `origin/main`, https://github.com/anubhav8it/TeamForge). `git` is not on PATH, so use the copy bundled with GitHub Desktop: `C:\Users\anubh\AppData\Local\GitHubDesktop\app-3.6.1\resources\app\git\cmd\git.exe`.

```powershell
$env:PATH = "D:\QT\Tools\mingw1310_64\bin;D:\QT\Tools\Ninja;D:\QT\Tools\CMake_64\bin;D:\QT\6.12.0\mingw_64\bin;" + $env:PATH
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=D:/QT/6.12.0/mingw_64 -DCMAKE_CXX_COMPILER=D:/QT/Tools/mingw1310_64/bin/g++.exe
cmake --build build
ctest --test-dir build --output-on-failure                   # all test executables
ctest --test-dir build -R tst_scoring --output-on-failure    # one executable
.\build\tst_dsa.exe bstPrefixSearch -o out.txt,txt           # one test function
cmake --build build --target all_qmllint                     # QML lint; keep at zero findings
.\build\TeamForge.exe                                        # needs D:\QT\6.12.0\mingw_64\bin on PATH
```

- `qt_standard_project_setup` puts **all executables in `build\`**, including the tests, not in `build\tests\`.
- When the test executables run directly from the PowerShell tool, their console output can be lost. Use `ctest`, or `-o <file>,txt`.
- The build is warning-free under `-Wall -Wextra -Wpedantic`. These flags come from the `teamforge_warnings` interface target; keep the build warning-free.
- **Portable test build** (`TeamForge-Test-Build/`, untracked): configure a Release build *outside* the repo, copy `TeamForge.exe`, `strip --strip-all` it (drops ~23 MB of symbols), run `windeployqt6 --release --qmldir ui --no-translations --compiler-runtime`, delete `qmltooling/` and `Qt6Quick3DUtils.dll` (debugger only), and add a `data/` folder holding the five seed files plus `.sample-data` from one clean run (no log, session or backups). It ships only the `windows` platform plugin, so verify it with a real window, not `offscreen`.
- To run the app without a window, set `$env:QT_QPA_PLATFORM='offscreen'`. If `ui/Main.qml` fails to load, the app exits with -1. `TeamForge.exe` is a GUI program, so set `QT_FORCE_STDERR_LOGGING=1` to see QML warnings.

## Layout and build targets

- **`teamforge_core`** is a static library built from `src/core`.
  - Headers are included as `"core/Xyz.h"`, and all code is in `namespace teamforge`.
  - Everything is standard C++17 **except `JsonStorage.cpp`**, the only file that uses Qt (Qt Core JSON). No core header includes Qt; keep it that way, so the course-relevant DSA and OOP code stays free of framework types.
- **`teamforge_ui`** is a static library holding the QML module `URI TeamForge`: all of `ui/`, the bridge in `src/app`, the logos, the fonts and the bundled sample data.
  - `teamforge` is the app (`TeamForge.exe`); it is just `src/main.cpp` plus `Q_IMPORT_QML_PLUGIN(TeamForgePlugin)`, and it loads `loadFromModule("TeamForge", "Main")`.
  - New `.qml` files go under `ui/` and must be added to `QML_FILES`.
  - The project needs Qt 6.8 or later, because policy QTP0004 requires it for QML in subdirectories.
- **`tests/`** has one Qt Test executable per area, registered with `teamforge_add_test(...)`:
  - `tst_domain`, `tst_scoring`, `tst_dsa`, `tst_engine` and `tst_persistence` test the core.
  - `tst_controller` tests the bridge; it compiles the `src/app` sources directly, with no QML engine.
  - `tst_ui` loads the real `Main.qml` offscreen and drives it with Qt mouse events: the entry screen and its 3D logo, workspaces and which controls each one shows, developer previews, the full participant and skill databases, hover states and tooltips.
  - Shared fixtures live in `tests/TestSupport.h`: students `studentA()`–`studentE()` and `scenarioRequirement()`. Their expected scores are worked out by hand in comments in `tst_scoring.cpp` and `tst_engine.cpp`, so a change to the scoring formula must update those comments as well as the numbers.
  - `TEAMFORGE_SOURCE_DIR` is defined for tests that read `data/`.
- **Don't name anything `slots`, `signals` or `emit`** in code that includes Qt headers. Qt defines them as macros. A test helper named `slots()` once broke compilation, and it is now `weekSlots()`.
- **`data/`** holds the committed sample data:
  - Files: `students.json`, `requirements.json`, `teams.json`, `skills.json` and `interest_requests.json`, each in the form `{"version": 1, "<collection>": [...]}`. Loaders ignore unknown fields, such as `"note"`.
  - Contents: 560 participants, 8 projects (CampusConnect, CivicLens, AgriVision, AlumniLoop, LangBridge, GreenGrid, SecureStack, HealthRoute) and 549 distinct skills in 29 categories.
  - `interest_requests.json` holds the seeded interest requests (`{id, studentId, requirementId, status}`, statuses `interested`, `under_review`, `accepted`, `declined`), so both sides of the interest workflow have data from the first run.
  - `TF-P001` (Anubhav Bisht's record) is the **demo participant**; the demo host is `TF-H001` "TeamForge Demo Host" (a constant in `BridgeSupport.h`; the host workspace always acts as the one local host). No passwords.
  - There is **no availability or scheduling** anywhere: the generator writes none, the loaders ignore old `availability` / `timeSlots` fields, and the next save drops them.
  - `skills.json` is the skill catalogue: `{"name", "category"}` per skill. It is display metadata only (the developer Skills page and autocomplete); skills stay open-ended, and a skill without an entry is "Uncategorised".
  - It is produced by `tools/generate_sample_data.py`, which is seeded (so the output is reproducible), asserts at least 500 distinct skills and that every catalogue skill is offered by someone. Summaries follow each profile's actual levels, so a beginner is never "experienced".
  - The generator keeps the first 12 original records. Everything else is generated, so edit the data through the script, never by hand.
  - The four team members' levels are **placeholders**. All other participants are generated profiles, not real students; the JSON `note` says so, and the UI never labels them.
  - Each generated profile's `role` is an internal skill track, used only by the diversity factor.
  - Students also have optional `summary` (a one-line skills-and-experience sentence) and `program` (academic details), and projects an optional `summary` tagline. All three are for display only and don't affect matching.

## Core architecture

There are two inputs: a `Student` profile and a `ProjectRequirement`. The pipeline runs in `MatchingEngine`, the facade that the UI should call:

```
retrieveCandidates (SkillIndex) → MatchingStrategy::evaluate → rankTopK (priority_queue) → suggestTeam (greedy) → JsonStorage
```

- **`MatchingEngine`** owns a `Repository<Student>`. It keeps three structures in sync on every add, update and remove:
  - `SkillIndex`: an `unordered_map` from skill to student ids, used for retrieval.
  - `SkillBST`: an AVL-balanced tree of skill names with counts, used for the sorted skill list and prefix autocomplete.
  - `SkillGraph`: an undirected skill co-occurrence graph, where an edge means one student offers both skills. Breadth-first search over it powers `relatedSkills` and `shortestPath`.

  Any new mutation path must update all three structures. `rebuildSkillIndex/Tree/Graph()` rebuild one structure from the repository; they exist for the developer workspace and must leave it equal to the incremental state (tested).
- **Skills are open-ended.** There is no master skill list:
  - Any string is a skill once `normalizeSkill` has trimmed it, collapsed its whitespace and lowercased it.
  - A new skill (e.g. "ROS2", "Kubernetes Operators") enters the index, the tree and the graph as soon as a profile offers it, and projects can require it straight away.
  - Normalised names are the keys everywhere. Levels run from 1 to 5.
- **Scoring.** The abstract `MatchingStrategy` has a protected `analyse()` that computes the five factors once. Concrete strategies only combine them:
  - `WeightedMatchingStrategy` uses `MatchWeights`, validated to sum to 1: **coverage 40%, complementarity 25%, experience 15%, engagement 10%, diversity 10%**.
  - **Engagement replaced availability** (an intentional product change: scheduling was removed). It is the participant's interest in the project, from an `EngagementSource` (an abstract interface; `InterestBook` implements it and the bridge sets it on both strategies): **1.0** once they have expressed interest in that project (whatever the host decided since), **0.5** (neutral) otherwise or with no source. Nothing of availability remains in the score.
  - `CoverageOnlyStrategy` is the evaluation baseline; only the developer workspace can switch to it.

  **The authoritative factor definitions are in the comment at the top of `MatchingStrategy.h`.** Strategies score a candidate against a `Team`, a partial team plus its requirement, because complementarity and diversity depend on who is already in the team. A participant's fit for a project is the same evaluation against an empty team.
- **Ranking.** `rankTopK` keeps a size-k min-heap, so ranking costs O(n log k). `MatchResult` defines `operator<` as "ranks below": lower score, with ties broken by the larger id. That keeps the order deterministic.
- **`suggestTeam`** is greedy: it repeatedly adds the top candidate until the team is full, or it covers every required skill and meets the minimum size. It is not guaranteed optimal.
- **`Team`** enforces no duplicate members and the maximum size on every add. `validateForFinalization()` checks only the minimum size. Missing skills are reported but don't block finalisation, because students make the final call.
- **Errors.** Every exception derives from `TeamForgeError`: `ValidationError`, `NotFoundError`, `DuplicateError`, `TeamConstraintError` and `PersistenceError`. The bridge adds `bridge::PermissionError`. Constructors validate their input. `JsonStorage` rethrows any record error as `PersistenceError` with the file name and record number.

## Workspaces and permissions

The app always starts on the entry screen (`ui/pages/EntryScreen.qml`). There are exactly three workspaces. The choice is local: no accounts, passwords or server.

| Workspace | Pages | Can | Cannot |
|---|---|---|---|
| Participant | Home, My profile, Opportunities | Create (four-step onboarding: Profile, Skills, Experience, Ready), claim their profile or continue as the demo participant; edit their skills, levels, summary and learning interests; see projects ranked by fit; **express interest** and follow its status; open Match insights for their own fit | Review requests, edit projects or others' profiles, build teams, use developer tools, change the strategy, reset data |
| Host | Overview, Projects, Applicants, Participants, Matching (team building lives in Matching) | Create, edit and delete projects; **review interest requests and accept or decline them**; browse participants; rank (everyone, or only accepted applicants), add, remove, suggest and save teams | Express interest, edit profiles, change the strategy or weights, use developer tools, reset data |
| Developer | Overview, Participants, Projects, Skills, Workspace | Everything: the complete, numbered participant, project, team and skill databases; a structured participant inspector with a Raw data tab (view, edit, save); projects with their applicants; create, rename or merge, recategorise and delete skills; previews as any participant or as the host with any project; save, reload, reset to the seed data; System tools (validation, structure rebuilds, persistence and matching checks, latest error, activity log) | n/a |

- **Permissions are enforced in C++**, not only hidden in QML. Every action calls `require(Permission)`; read-only developer queries (`queryParticipants`, `querySkills`, `skillDetail`, `inspectRecord`) return nothing outside the developer workspace (`allowed()`). On the entry screen no action is allowed. `tst_controller` and `tst_ui` cover both the rules and the visible controls.
- **Developer preview** uses exactly the previewed workspace's permissions, behind a "DEVELOPER PREVIEW" banner that can switch between the participant and host previews. `previewWorkspace(workspace, identity)` takes who to be: a participant id (default: the session's own profile, else the demo participant via `pickPreviewProfile()`), or a project to open for the host. It needs no onboarding; the chosen profile is held in `previewProfileId_` and never written to the session.
- **Interest workflow** (`InterestBook` in the core, `TeamForgeInterest.cpp` and `expressInterest` in the bridge). A participant expresses interest (`interested`); the host opens it ("Why this match?" marks it `under_review`) and accepts or declines; a host may switch accepted and declined later. Transitions are checked in `InterestRequest::canMove`. Accepting does **not** add anyone to a team: accepted people are listed in Matching (`acceptedCandidates`) for the host to add. Every change is saved immediately.
- **Database numbering.** Records keep their stable ids; the bridge adds a display `number` (participants and skills in A–Z order, projects in id order, saved teams in save order). Filtering and sorting never renumber.
- **Errors by role.** Participants and hosts get a short, friendly `lastError` (`friendlyMessage()`), never paths, ids or exception names. The developer workspace sees the backend message. Full details for every error (type, operation, record, workspace, storage location, time, log line) are kept in `lastErrorDetail` for the session and shown under Developer → Workspace → System tools → Latest error (and on the Overview).
- **Local session.** `<data dir>/session.json` keeps the last workspace, the participant's own profile id (`myProfileId`), and the recent-project order, so they persist across restarts. A missing or unreadable file just means a fresh session.
- **Local log.** `AppLog` keeps the last 400 entries in memory (Developer → Workspace → System tools → Activity) and appends to `<data dir>/teamforge.log`, rotating at 512 KB. It records startup, workspace changes, profile, project and team changes, matching selections, saves and loads, every error, and developer actions. Nothing leaves the machine.

## QML bridge

`TeamForgeController` (`src/app/`) is exposed to QML as the singleton **`Backend`** (`import TeamForge`). Its sources:
- `TeamForgeController.cpp`: data, host actions, workspaces, errors and the session.
- `TeamForgeInterest.cpp`: the host side of the interest workflow, accepted candidates and the demo identities.
- `TeamForgeParticipant.cpp`: my profile, opportunities, profile completion and the directory search.
- `TeamForgeDeveloper.cpp`: system status, maintenance actions and the self-checks.
- `TeamForgeRecords.cpp`: the developer databases (`queryParticipants`, `querySkills`, `skillDetail`), skill catalogue edits (`saveSkill`, `renameSkill`, `deleteSkill`, all or nothing through `rewriteSkills`, so the three structures stay in sync) and the raw-record inspector (`inspectRecord`, `applyRecordJson`). Raw records use `storage::toJson` / `studentFromJson` / `requirementFromJson`, so they are exactly the stored format with the loaders' validation; stored and loaded copies are compared after a canonical round trip.
- Helpers: `BridgeSupport` (map conversions and `PermissionError`), `AppLog` and `SampleData`.

How it works:
- **It only adapts the core.** It converts core objects into `QVariantMap`/`QVariantList` with the same keys as the JSON files, and calls `MatchingEngine`, `Team` and `storage::`. No scoring, ranking or validation rule may live in the bridge or in QML. `tst_controller` checks the bridge against the core:
  - its rankings and suggestions match `MatchingEngine` run directly on the same data
  - participant fits match the weighted strategy's own evaluation
- **Errors never reach QML as exceptions.** Every action runs through `run(operation, subject, action)`, which catches `TeamForgeError`, records the details, logs them, sets the role-appropriate `lastError` and returns `false`. Read-only queries pass `clearErrorOnSuccess = false`, so they never hide an error.
- **State.** The bridge holds:
  - the engine, which owns the students
  - `Repository<ProjectRequirement>`
  - saved teams, stored as id records and rebuilt into `Team` objects against the current profiles
  - the current team, stored as member ids plus the selected requirement
  - the workspace and session

  `refresh()` rebuilds the current `Team` and the ranking, then emits `selectionChanged`. The bridge refuses edits that would break saved teams: removing a member student, removing a requirement in use, or shrinking a requirement's maximum size below a saved team's size.
- **Caching.** `students` (pre-sorted by name, plus a parallel search index), `requirements` and `opportunities` are cached and rebuilt only on `notifyDataChanged()`. Every data mutation must go through it, never through a bare `emit dataChanged()`.
- **Autocomplete.** `skillsWithPrefix` is the AVL tree's prefix search, merged with catalogued and project-required skills, so a skill autocompletes before anyone offers it.
- **Search.** `searchStudents(query, skill, sort)` runs in C++. A name matches by substring. A skill matches if the query starts the skill, starts one of its words, or equals its initials. So "ML" finds "machine learning" but not "HTML/CSS".
- **Read models.** These are computed in C++:
  - requirement entries carry `type`, `summary`, `candidateCount`, `savedTeamCount`, and a per-skill `qualifiedCount` and `offeredCount`
  - match results carry a `skillBreakdown` and per-factor contributions
  - teams carry `membersNeeded`
  - opportunities carry `score`, `keySkills`, `coveredCount` and `sharedSlots`; `myProfile` carries `recentMatches` (saved teams the participant is in, then recently opened opportunities)
  - students carry `experience` (Beginner .. Advanced), `topSkills` and `number`; requirements carry `number` and `interest` counts per status; opportunities carry `interestStatus` and `interestId`; `myInterests` lists the participant's requests; `projectInterests(id)` a project's requests with fit scores
  - `profileCompletion()` gives the completion ratio and checklist, for a saved profile or an onboarding draft
  - `composeSummary()` drafts a one-line profile sentence
- **Factor labels** live in `kFactors`. The `shortLabel` is what users see (Skill fit, Team fit, Experience, Engagement, Balance); the `label` is the full term (Coverage, Complementarity, Experience, Engagement / interest, Diversity / balance). The definitions mirror `MatchingStrategy.h`; update both together.
- **Runtime data folder.** The app reads and saves `<exe dir>/data` (`TEAMFORGE_DATA_DIR` overrides it); if that folder can't be written (e.g. unpacked under Program Files) it falls back to `%LOCALAPPDATA%\TeamForge\data`. It never writes the repository's `data/`. The committed samples are bundled into the module (`RESOURCES`). On startup `sampledata::ensureSeeded` compares the folder's `.sample-data` stamp (a SHA-256 of the four sample files) with the bundled samples:
  - If they match, the folder is left alone, so teams and profiles saved in the app survive restarts and rebuilds. Missing files are restored.
  - If they differ, or the folder has no stamp, all four files are replaced together, after any that differ are copied to `backup-<timestamp>/`.

  Rebuilding embeds the current samples, so every build folder, including Qt Creator's `build/Desktop_Qt_…`, picks up regenerated data on its next run; that folder once kept showing stale 12-person data. Developer → Workspace → "Reset seed data" does the same replacement unconditionally (with a backup and a confirmation dialog).

## UI

Qt Quick with the **Basic** controls style (`import QtQuick.Controls.Basic` in every file, so styling is under our control). The files:
- `ui/Main.qml`: the shell. It shows the entry screen or the active workspace, loaded through a `Loader` so only that workspace's pages exist. It also owns the sidebar, top bar, preview banner, error banner, toast and the shared `MatchInsightsDrawer`.
- `ui/Theme.qml`: a singleton holding all colours, the type scale, the logo URLs and presentation helpers: `count()`, `percent()`, `skill()`, `experienceLabel()`, `levelName()`, `fitSummary()`, `avatarStyle()`, `scoreColor()`, `typeColor()` and `workspaceColor()`/`workspaceName()`.
- `ui/components/`: reusable themed controls:
  - controls: `Tf*`, `Panel`, `Chip`, `SkillChips` ("+N more"), `Segmented`, `SkillField` (open-ended text with prefix autocomplete), `ConfirmDialog`. `TfComboBox` has a real text field, so `editable: true` works (read `editText`).
  - `LogoMark3D`: the entry screen's logo with depth (see "Logos")
  - figures and indicators: `LevelPips`, `MeterBar`, `CoverageRing`, `StatTile`, `CheckResult`
  - chrome: `Sidebar`, `NavItem`, `TopBar`, `ErrorBanner`, `Toast`
- `ui/pages/`: `EntryScreen` (backdrop of drifting colour fields and a dot grid with pointer parallax, the 3D logo, wordmark and three workspace cards that tilt towards the pointer; the entrance replays each time it is shown), and `MatchInsightsDrawer` ("Why this match?"), which shows a host's candidate against the current team or a participant's fit with a project. Main closes the drawer on every workspace change.
- `ui/participant/`: `ParticipantWorkspace`, `ParticipantHome`, `MyProfilePage`, `OpportunitiesPage`, `OpportunityCard`, `OpportunityDrawer`, `ProfileEditor` (onboarding and edit, steps 01–04) and `ProfilePicker`.
- `ui/host/`: `HostWorkspace`, `OverviewPage`, `ProjectsPage`, `ParticipantsPage`, `MatchingPage` and `TeamBuilderPanel`.
- `ui/host/` also has `ApplicantsPage` and `InterestReviewList` (a project's requests by stage, with Accept and Decline), which the developer's project view reuses in a drawer.
- `ui/developer/`: `DeveloperWorkspace`, `Dev{Overview,Participants,Skills,Workspace}Page` and the shared `RecordInspector` drawer: participants open on a structured view (Profile, Skills, Learning, Matching data) with a Raw data tab; other records on the raw views (record, on disk, indexes, matching). The Projects page is the host's `ProjectsPage` with `developerMode: true`, which adds ids, saved teams, Applicants and "Raw record". Technical tools sit under Workspace → System tools, folded away by default.

Every new `.qml` file must be listed in `QML_FILES` in the root `CMakeLists.txt`.

Rules for the QML:
- **QML is presentation only.** It displays bridge data, keeps form drafts and calls bridge actions. Scoring, ranking, search, permissions, completion rules, validation and persistence stay in C++. If a screen needs a derived value, add it to the bridge's read models.
- **Matching rows stay scannable:** avatar, name, 2–3 strongest matching skills, match %, "Why this match?" and "+ Add", with no explanatory sentences. The full breakdown lives in Match insights. Add buttons ignore a second click for 400 ms, because the list re-ranks under the pointer after an add.
- **Never display participant roles.** The bridge still exposes `role` because the diversity factor uses it.
- **No availability or scheduling in the UI.** It was removed from the product (and the score); `tst_ui` checks that no participant or host page mentions availability, sessions or time slots.
- **Text** uses `TfText` (plain-text rendering, so data can't inject markup). Plurals go through `Theme.count()`, because no translation catalogue is loaded and `qsTr`'s `%n` would print "(s)".
- **Form fields** react to `textChanged` with a guard, not to `textEdited`. Otherwise values set by assistive technology are missed. This was a real bug.
- **Layout gotchas.**
  - A child with `Layout.fillWidth: true` makes its parent layout fill too. Fixed-width columns therefore need explicit `Layout.fillWidth: false` or a small `Layout.preferredWidth`.
  - Drawers compute one `panelWidth` and use it for both `width` and `contentWidth`. `contentWidth: width` causes a binding loop.
- **Logos.** `assets/teamforge_logo.png` is the original. `assets/teamforge_logo_dark.png` (full logo) and `teamforge_mark_dark.png` (TF monogram) are recoloured versions for the dark UI, bundled as QML-module `RESOURCES`. The mark is also the window icon. The entry screen uses three full-resolution layers of the monogram (`teamforge_mark_{light,red,depth}.png`), cut from the original by `tools/make_logo_layers.ps1`; `LogoMark3D` stacks them with a perspective `Rotation` (Qt Quick, no 3D model) for depth and parallax: it tilts towards the pointer, turns when dragged, spins 360° when clicked and springs back.
- **Accessibility.** Custom clickable items set `Accessible.role`, `Accessible.name` and `Accessible.onPressAction`, and text fields take their accessible name from `placeholderText`. This also lets UI Automation drive the app.
- **Typography.** The UI font is **Manrope**, bundled in five static weights from Regular to ExtraBold. The files are `assets/fonts/*.ttf`, under the SIL Open Font License in `assets/fonts/OFL.txt`; `main.cpp` registers them. Hierarchy comes from weight and size: ExtraBold for titles, names and scores; Bold for headings, navigation and buttons; Medium for body text. Body text is 16 px; the minimum size is 13 px. No all-caps sentences: capitals are for skills and the three workspace names on the entry screen.
- **Skills are always displayed in capitals.** Use `Chip { skill: true }`, `SkillChips` or `Theme.skill(name)`. The stored, normalised names stay lower case, so never compare against the displayed text.
- **Colour carries meaning.**
  - red: primary action, the host workspace, coverage, missing skills
  - green: met or covered
  - blue: engagement and interest, and the participant workspace
  - amber: experience and below-minimum
  - teal: complementarity
  - violet: diversity and the developer workspace

  The factor colours are `Theme.factorColors`, in `factorDefinitions` order.
- **Avatars** are generated by `Avatar.qml`: white initials on a gradient disc. `Theme.avatarStyle()` picks the colour pair and angle deterministically from the participant id; `hash()` returns a real, because an `int` would wrap negative. A red ring means "in the team"; a white ring means "selected or explained". There are no image files. Always pass `seed: <id>`, so a person looks the same everywhere.
- **Long lists.** List views use `reuseItems`, and search applies 150 ms after typing stops. Chip rows sit in a fixed-height, clipped `Flow`, so overflowing chips disappear whole rather than overlapping the next column.
- **Window sizes.** Check layouts at 1280×720 and 1920×1080 logical. Below a width of 1360 the sidebar collapses to icons. The layouts avoid fixed phone-hostile assumptions, but there is no separate mobile app.

## What TeamForge is

TeamForge is a C++/Qt desktop app that matches students to hackathon or project teams by **complementary** skills, not similar ones. It is a PBL project for the CSE department at Graphic Era (Team DSCPP-III-2026-T238, Semester III, session 2026–27). The roadmap has three phases:
- Phase I: design (done)
- Phase II: build the app and the matching engine
- Phase III: testing, refinement and the final demo

Each feature must show a meaningful use of both courses:
- **TCS-302, Data Structures:** hashing, searching and sorting, queues and priority queues, trees (BST/AVL), linked structures and graphs.
- **TCS-307, OOP with C++:** classes, inheritance, abstract classes, virtual functions, templates, operator overloading, exceptions and file I/O.

Prefer designs where these concepts do real work, and say which concept each piece demonstrates. The developer workspace's Skills page (every skill with its SkillIndex, SkillBST and SkillGraph state, related skills by BFS), the record inspector's Indexes and Matching data tabs and Workspace → System tools (rebuilds, consistency checks) put the DSA structures on screen.

The match weights are coverage 40%, complementarity 25%, experience 15%, engagement 10% and diversity 10%. The Phase-I report had availability where engagement now is; availability was removed from the product on purpose and engagement took its weight. Don't change the defaults. The score supports a decision rather than making it, so the UI must show *why* a candidate ranks where they do: Match insights shows the factor breakdown and the skill-by-skill comparison. Matching logic stays in C++, never in QML.

## Team roles

Use these to know whose area a change touches:
- Mukul Fartiyal: integration and architecture
- Anubhav Bisht: Qt UI, testing and documentation
- Siddhant Singh: C++/OOP class design
- Mehul: DSA, meaning matching, hashing and ranking
