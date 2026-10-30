// Resume import. Part of TeamForgeController. Text extraction and analysis are core code
// (core/DocumentText.h, core/ResumeAnalyzer.h); this reads the file, runs the work off the UI
// thread and turns the analysis into a map the review screen shows.
//
// Privacy: the file stays in memory while it is reviewed and is never written to the shared
// data files. Only the details the person confirms are saved (saveMyProfile). A desktop build
// can also keep the file as a private copy in the local data folder, on request; the browser
// build never keeps it. The log records the file type, size and counts, never names or text.
#include "TeamForgeController.h"

#include "BridgeSupport.h"

#include "core/DocumentText.h"
#include "core/Errors.h"
#include "core/ResumeAnalyzer.h"
#include "core/Skill.h"

#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTimer>

#include <algorithm>
#include <set>

#if defined(Q_OS_WASM)
#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>
#endif

using namespace teamforge;
using namespace bridge;

namespace {

QVariantMap analysisToMap(const ResumeAnalysis& a, const storage::SkillCatalog& catalog)
{
    QVariantList skills;
    for (const ResumeSkill& s : a.skills) {
        const auto category = catalog.find(s.skill);
        skills.append(QVariantMap{
            {QStringLiteral("skill"), qs(s.skill)},
            {QStringLiteral("category"), category != catalog.end() && !category->second.empty()
                                             ? qs(category->second) : QStringLiteral("Uncategorised")},
            {QStringLiteral("evidence"), qs(s.evidence)},
            {QStringLiteral("years"), s.years},
            {QStringLiteral("yearsEvidence"), qs(s.yearsEvidence)},
            {QStringLiteral("suggestedLevel"), s.suggestedLevel},
        });
    }
    return {
        {QStringLiteral("name"), qs(a.name)},
        {QStringLiteral("nameEvidence"), qs(a.nameEvidence)},
        {QStringLiteral("age"), a.age},
        {QStringLiteral("ageEvidence"), qs(a.ageEvidence)},
        {QStringLiteral("totalYears"), a.totalYears},
        {QStringLiteral("totalYearsEvidence"), qs(a.totalYearsEvidence)},
        {QStringLiteral("skills"), skills},
        {QStringLiteral("unmatchedSkills"), toQStringList(a.unmatchedSkills)},
        {QStringLiteral("projects"), toQStringList(a.projects)},
        {QStringLiteral("education"), toQStringList(a.education)},
        {QStringLiteral("certifications"), toQStringList(a.certifications)},
        {QStringLiteral("achievements"), toQStringList(a.achievements)},
    };
}

QString sizeText(qint64 bytes)
{
    return bytes >= 1024 * 1024 ? QStringLiteral("%1 MB").arg(static_cast<double>(bytes) / (1024.0 * 1024.0), 0, 'f', 1)
                                : QStringLiteral("%1 KB").arg(std::max<qint64>(1, bytes / 1024));
}

#if defined(Q_OS_WASM)
// The controller that asked for the browser's file picker.
QPointer<TeamForgeController>& pickerTarget()
{
    static QPointer<TeamForgeController> target;
    return target;
}

// A hidden <input type=file>; the chosen file comes back through the two functions below.
EM_JS(void, tf_pick_resume, (double maxBytes), {
    var input = document.createElement('input');
    input.type = 'file';
    input.accept = '.pdf,.docx,application/pdf,application/vnd.openxmlformats-officedocument.wordprocessingml.document';
    input.style.display = 'none';
    input.onchange = function () {
        var file = input.files && input.files[0];
        input.remove();
        if (!file)
            return;
        if (file.size > maxBytes) {
            Module.tfResumeFileRejected(file.name, file.size);
            return;
        }
        file.arrayBuffer().then(function (buffer) {
            Module.tfResumeFileLoaded(file.name, new Uint8Array(buffer));
        }).catch(function () {
            Module.tfResumeFileRejected(file.name, -1);
        });
    };
    document.body.appendChild(input);
    input.click();
});

void resumeFileLoaded(const std::string& name, const emscripten::val& data)
{
    TeamForgeController *target = pickerTarget().data();
    if (target == nullptr)
        return;
    const auto length = data["length"].as<unsigned>();
    QByteArray bytes(static_cast<qsizetype>(length), Qt::Uninitialized);
    emscripten::val(emscripten::typed_memory_view(length, reinterpret_cast<unsigned char *>(bytes.data())))
        .call<void>("set", data);
    target->analyseResumeData(QString::fromStdString(name), bytes);
}

// Too large, or unreadable: passed on as an empty file, which analyseResumeData reports.
void resumeFileRejected(const std::string& name, double /*size*/)
{
    if (TeamForgeController *target = pickerTarget().data())
        target->analyseResumeData(QString::fromStdString(name), QByteArray());
}

EMSCRIPTEN_BINDINGS(teamforge_resume)
{
    emscripten::function("tfResumeFileLoaded", &resumeFileLoaded);
    emscripten::function("tfResumeFileRejected", &resumeFileRejected);
}
#endif

} // namespace

int TeamForgeController::resumeMaxMegabytes() const
{
    bool ok = false;
    const int fromEnvironment = qEnvironmentVariableIntValue("TEAMFORGE_RESUME_MAX_MB", &ok);
    return ok && fromEnvironment > 0 ? fromEnvironment : 20;
}

QVariantMap TeamForgeController::resume() const
{
    QVariantMap map = resume_;
    if (!map.contains(QStringLiteral("state")))
        map.insert(QStringLiteral("state"), QStringLiteral("idle"));
#if defined(Q_OS_WASM)
    map.insert(QStringLiteral("canKeepCopy"), false);
    map.insert(QStringLiteral("hasCopy"), false);
#else
    const QString path = resumeCopyPath();
    map.insert(QStringLiteral("canKeepCopy"), !resumeBytes_.isEmpty() && !path.isEmpty());
    map.insert(QStringLiteral("hasCopy"), !path.isEmpty()
                                              && (QFile::exists(path + QStringLiteral(".pdf"))
                                                  || QFile::exists(path + QStringLiteral(".docx"))));
#endif
    return map;
}

void TeamForgeController::setResumeState(const QString& state, double progress, const QString& stage)
{
    resume_.insert(QStringLiteral("state"), state);
    resume_.insert(QStringLiteral("progress"), progress);
    resume_.insert(QStringLiteral("stage"), stage);
    emit resumeChanged();
}

bool TeamForgeController::analyseResumeFile(const QUrl& file)
{
    const QString path = file.isLocalFile() ? file.toLocalFile() : file.toString();
    const QFileInfo info(path);
    QByteArray bytes;
    QString problem;
    if (!info.isFile()) {
        problem = QStringLiteral("That file could not be found.");
    } else if (info.size() > qint64(resumeMaxMegabytes()) * 1024 * 1024) {
        problem = QStringLiteral("This file is %1; resumes can be up to %2 MB.").arg(sizeText(info.size())).arg(resumeMaxMegabytes());
    } else {
        QFile in(path);
        if (!in.open(QIODevice::ReadOnly))
            problem = QStringLiteral("That file could not be opened.");
        else
            bytes = in.readAll();
    }
    if (!problem.isEmpty()) {
        if (!run("analyseResume", QString(), [&] { require(Permission::EditOwnProfile); }, false))
            return false;
        ++resumeJob_;
        resume_ = {{QStringLiteral("fileName"), info.fileName()}, {QStringLiteral("error"), problem}};
        setResumeState(QStringLiteral("error"), 0, QString());
        return false;
    }
    return analyseResumeData(info.fileName(), bytes);
}

bool TeamForgeController::analyseResumeData(const QString& fileName, const QByteArray& bytes)
{
    if (!run("analyseResume", QString(), [&] { require(Permission::EditOwnProfile); }, false))
        return false;

    const int job = ++resumeJob_;
    resumeBytes_.clear();
    resumeFileName_ = fileName;
    resume_ = {{QStringLiteral("fileName"), fileName}, {QStringLiteral("sizeBytes"), bytes.size()},
               {QStringLiteral("sizeText"), sizeText(bytes.size())}};

    // Quick checks first, so a wrong file is reported at once.
    QString problem;
    if (bytes.isEmpty())
        problem = QStringLiteral("This file could not be read, or it is larger than %1 MB.").arg(resumeMaxMegabytes());
    else if (bytes.size() > qsizetype(resumeMaxMegabytes()) * 1024 * 1024)
        problem = QStringLiteral("This file is %1; resumes can be up to %2 MB.").arg(sizeText(bytes.size())).arg(resumeMaxMegabytes());
    else {
        try {
            detectDocumentType(std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size())), ss(fileName));
        } catch (const DocumentError& error) {
            problem = QString::fromUtf8(error.what());
        }
    }
    if (!problem.isEmpty()) {
        resume_.insert(QStringLiteral("error"), problem);
        setResumeState(QStringLiteral("error"), 0, QString());
        logEvent(AppLog::Level::Warning, QStringLiteral("resume"), QStringLiteral("Resume rejected before analysis"));
        return false;
    }

    resumeBytes_ = bytes;
    setResumeState(QStringLiteral("working"), 0.15, QStringLiteral("Reading the file"));

    // Everything the work needs is copied in: the worker never touches the controller's state.
    std::set<std::string> known;
    for (const auto& [skill, category] : skillCatalog_) {
        (void)category;
        known.insert(skill);
    }
    for (const std::string& skill : engine_.skillsWithPrefix(""))
        known.insert(skill);
    for (const ProjectRequirement& requirement : requirements_.values())
        for (const auto& [skill, level] : requirement.requiredSkills()) {
            (void)level;
            known.insert(skill);
        }
    const QDate today = QDate::currentDate();
    TeamForgeController *self = this;
    auto work = [self, job, data = bytes.toStdString(), name = ss(fileName),
                 known = std::vector<std::string>(known.begin(), known.end()), catalog = skillCatalog_,
                 today = YearMonth{today.year(), today.month()}] {
        const auto stage = [self, job](double progress, const QString& text) {
            QMetaObject::invokeMethod(self, [self, job, progress, text] {
                if (self->resumeJob_ == job)
                    self->setResumeState(QStringLiteral("working"), progress, text);
            }, Qt::QueuedConnection);
        };
        QVariantMap result;
        QString error;
        try {
            stage(0.4, QStringLiteral("Extracting the text"));
            const std::string text = documentText(data, name);
            stage(0.75, QStringLiteral("Finding skills and experience"));
            const RuleBasedResumeExtractor extractor(known);
            result = analysisToMap(extractor.analyse(text, today), catalog);
        } catch (const DocumentError& e) {
            error = QString::fromUtf8(e.what());
        } catch (const std::exception&) {
            error = QStringLiteral("This file could not be read. It may be damaged; try saving it again as PDF or DOCX.");
        }
        QMetaObject::invokeMethod(self, [self, job, result, error] { self->finishResume(job, result, error); },
                                  Qt::QueuedConnection);
    };
#if QT_CONFIG(thread)
    resumeWorkers_.start(work);
#else
    // Single-threaded browser build: let the progress state paint, then work in place.
    QTimer::singleShot(30, this, work);
#endif
    return true;
}

void TeamForgeController::finishResume(int job, const QVariantMap& result, const QString& error)
{
    if (job != resumeJob_)
        return; // cleared or replaced meanwhile
    if (!error.isEmpty()) {
        resumeBytes_.clear();
        resume_.insert(QStringLiteral("error"), error);
        setResumeState(QStringLiteral("error"), 0, QString());
        logEvent(AppLog::Level::Warning, QStringLiteral("resume"), QStringLiteral("Resume could not be analysed"));
        return;
    }
    resume_.insert(QStringLiteral("result"), result);
    resume_.remove(QStringLiteral("error"));
    setResumeState(QStringLiteral("ready"), 1.0, QStringLiteral("Ready to review"));
    logEvent(AppLog::Level::Info, QStringLiteral("resume"),
             QStringLiteral("Resume analysed (%1, %2): %3 known skills, %4 to review")
                 .arg(resumeFileName_.section(QLatin1Char('.'), -1).toUpper(), sizeText(resumeBytes_.size()))
                 .arg(result.value(QStringLiteral("skills")).toList().size())
                 .arg(result.value(QStringLiteral("unmatchedSkills")).toList().size()));
}

void TeamForgeController::chooseResumeFile()
{
#if defined(Q_OS_WASM)
    if (!run("chooseResumeFile", QString(), [&] { require(Permission::EditOwnProfile); }, false))
        return;
    pickerTarget() = this;
    tf_pick_resume(double(resumeMaxMegabytes()) * 1024.0 * 1024.0);
#endif
}

void TeamForgeController::clearResume()
{
    ++resumeJob_;
    resumeBytes_.clear();
    resumeFileName_.clear();
    resume_.clear();
    emit resumeChanged();
}

QString TeamForgeController::resumeCopyPath() const
{
    const std::string& id = ownProfileId();
    if (id.empty())
        return {};
    QString safe = qs(id);
    safe.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]")), QStringLiteral("_"));
    return dataDirectory_ + QStringLiteral("/private/resume-") + safe;
}

bool TeamForgeController::keepResumeCopy()
{
    return run("keepResumeCopy", QString(), [&] {
        require(Permission::EditOwnProfile);
#if defined(Q_OS_WASM)
        throw ValidationError("The browser version does not keep resume files");
#else
        const QString base = resumeCopyPath();
        if (base.isEmpty())
            throw ValidationError("Save your profile before keeping a copy of your resume");
        if (resumeBytes_.isEmpty())
            throw ValidationError("There is no resume to keep");
        QDir().mkpath(QFileInfo(base).absolutePath());
        const bool pdf = detectDocumentType(std::string_view(resumeBytes_.constData(), static_cast<std::size_t>(resumeBytes_.size())),
                                            ss(resumeFileName_)) == DocumentType::Pdf;
        QFile::remove(base + (pdf ? QStringLiteral(".docx") : QStringLiteral(".pdf")));
        QSaveFile out(base + (pdf ? QStringLiteral(".pdf") : QStringLiteral(".docx")));
        if (!out.open(QIODevice::WriteOnly) || out.write(resumeBytes_) != resumeBytes_.size() || !out.commit())
            throw PersistenceError("Could not save the resume copy: " + ss(out.errorString()));
        logEvent(AppLog::Level::Info, QStringLiteral("resume"), QStringLiteral("Private resume copy kept"));
        emit resumeChanged();
#endif
    });
}

bool TeamForgeController::deleteResumeCopy()
{
    return run("deleteResumeCopy", QString(), [&] {
        require(Permission::EditOwnProfile);
        const QString base = resumeCopyPath();
        if (base.isEmpty())
            return;
        QFile::remove(base + QStringLiteral(".pdf"));
        QFile::remove(base + QStringLiteral(".docx"));
        logEvent(AppLog::Level::Info, QStringLiteral("resume"), QStringLiteral("Private resume copy deleted"));
        emit resumeChanged();
    });
}
