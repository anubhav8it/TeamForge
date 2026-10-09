#pragma once

#include <QString>
#include <QStringList>

// Keeps a runtime data folder in step with the committed sample data, which is bundled into
// the app (QML module RESOURCES, ":/qt/qml/TeamForge/data"). The folder records which sample
// set it was seeded from in `.sample-data` (a SHA-256 over the sample files):
//   - same samples: nothing is touched, so teams and edits saved in the app survive; missing
//     files are restored from the samples;
//   - samples changed, or no stamp (a folder seeded by an older build): the files are
//     replaced together, after copying any that differ to backup-<timestamp>/.
// Deterministic: the result depends only on the bundled samples and the folder's contents.
namespace sampledata {

inline const QStringList& files()
{
    static const QStringList names{QStringLiteral("students.json"), QStringLiteral("requirements.json"),
                                   QStringLiteral("teams.json"), QStringLiteral("skills.json"),
                                   QStringLiteral("interest_requests.json")};
    return names;
}

struct Result
{
    bool ok = true;
    bool replaced = false;
    QString message;   // one line for the log / developer workspace
    QString backupDir; // set when existing files were backed up
};

// Bundled samples: ":/qt/qml/TeamForge/data". Tests point this at the repository's data/.
QString defaultSampleDirectory();

Result ensureSeeded(const QString& dataDir, const QString& sampleDir = defaultSampleDirectory());
// Replaces the data files with the samples unconditionally (backing up any that differ).
Result restore(const QString& dataDir, const QString& sampleDir = defaultSampleDirectory());

} // namespace sampledata
