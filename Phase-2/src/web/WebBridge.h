#pragma once

#include <QObject>
#include <QString>

// Browser build only (Qt for WebAssembly); the desktop build does not compile this file.
//
// The page that loads TeamForge (web/static/app/index.html) signs the person in, fetches the
// shared data files and this account's session from the server, and leaves them in the
// JavaScript global `TF_BOOT` before the app starts:
//
//   TF_BOOT = { allowedWorkspaces: "participant,host",
//               files: { "students.json": "<file text>", ..., "session.json": "<file text>" } }
//
// prepareDataDirectory() writes those files, unchanged, into the app's data folder (Emscripten's
// in-memory file system) and sets TEAMFORGE_DATA_DIR and TEAMFORGE_ALLOWED_WORKSPACES. The
// controller, the C++ core, the seeding logic and the JSON file format therefore work exactly
// as on the desktop. startSync() then checks the folder regularly and passes every file whose
// content changed to the page's `TF_upload(name, text)`, which saves it on the server.
namespace webbridge {

// Call before the QML engine creates the Backend singleton. Returns the data folder.
QString prepareDataDirectory();

// Starts the periodic check, owned by `owner`.
void startSync(QObject *owner);

// One check right now. The page calls it as `instance.tfFlush()` before the tab closes.
void flush();

} // namespace webbridge
