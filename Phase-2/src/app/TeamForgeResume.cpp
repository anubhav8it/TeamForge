// Resume import. Part of TeamForgeController. Text extraction and analysis are core code
// (core/DocumentText.h, core/ScanQuality.h, core/ResumeAnalyzer.h); this reads the file, runs
// the work off the UI thread and turns the analysis into a map the review screen shows.
//
// Text documents (PDF, DOCX, ODT, RTF, text, HTML) are read here, in the app. A photo or a
// scanned PDF has no text to extract: a photo is first checked (size, light, contrast,
// sharpness) and re-encoded as JPEG, then the person is asked before it is sent to be read
// (OCR: Azure AI Document Intelligence, through the TeamForge server). What comes back is
// checked again: many uncertain words mean the picture was too blurry to trust.
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
#include "core/ScanQuality.h"
#include "core/Skill.h"

#include <QBuffer>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QJsonDocument>
#include <QPainter>
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

constexpr qint64 kDefaultReadMaxBytes = 4 * 1024 * 1024; // Azure's free tier
constexpr int kReadLongSide = 3000;                       // plenty for the reader, well under its limit

// A photo, checked and made ready to read: decoded (turned upright when the camera says so),
// refused when it is too small, dark, faint or blurry, and re-encoded as a JPEG of at most
// `maxBytes`. `info` gets {kind, width, height, sizeText}. Runs on a worker thread.
QByteArray preparePhoto(const QByteArray& data, qint64 maxBytes, QVariantMap& info)
{
    QBuffer buffer;
    buffer.setData(data);
    QImageReader reader(&buffer);
    reader.setAutoTransform(true);
    QImage image = reader.read();
    if (image.isNull())
        throw DocumentError("TeamForge couldn't open this image. Save it as a JPEG or PNG and upload it again.");
    // Transparent areas (a PNG export) become white paper, not black.
    QImage page(image.size(), QImage::Format_RGB32);
    page.fill(Qt::white);
    {
        QPainter painter(&page);
        painter.drawImage(0, 0, image);
    }

    QImage grey = page.convertToFormat(QImage::Format_Grayscale8);
    if (std::max(grey.width(), grey.height()) > kScanWorkingSide)
        grey = grey.scaled(kScanWorkingSide, kScanWorkingSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    GreyImage pixels{grey.width(), grey.height(), {}};
    pixels.pixels.reserve(static_cast<std::size_t>(grey.width()) * static_cast<std::size_t>(grey.height()));
    for (int y = 0; y < grey.height(); ++y)
        pixels.pixels.insert(pixels.pixels.end(), grey.constScanLine(y), grey.constScanLine(y) + grey.width());
    requireReadableScan(page.width(), page.height(), measureScan(pixels));

    if (std::max(page.width(), page.height()) > kReadLongSide)
        page = page.scaled(kReadLongSide, kReadLongSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QByteArray jpeg;
    for (int attempt = 0; attempt < 8; ++attempt) {
        jpeg.clear();
        QBuffer out(&jpeg);
        out.open(QIODevice::WriteOnly);
        page.save(&out, "JPG", attempt < 3 ? 88 - 15 * attempt : 60);
        if (jpeg.size() <= maxBytes)
            break;
        if (attempt >= 3)
            page = page.scaled(page.size() * 0.8, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    if (jpeg.isEmpty() || jpeg.size() > maxBytes)
        throw DocumentError("This image could not be made small enough to read. Use a smaller photo.");
    info = {{QStringLiteral("kind"), QStringLiteral("photo")},
            {QStringLiteral("width"), image.width()},
            {QStringLiteral("height"), image.height()},
            {QStringLiteral("sizeText"), sizeText(jpeg.size())}};
    return jpeg;
}

ResumeAnalysis analyseText(const std::string& text, const std::vector<std::string>& known, const QDate& today)
{
    const RuleBasedResumeExtractor extractor(known);
    return extractor.analyse(text, YearMonth{today.year(), today.month()});
}

#if defined(Q_OS_WASM)
// The controller that asked for the browser's file picker, or for a picture to be read.
QPointer<TeamForgeController>& pickerTarget()
{
    static QPointer<TeamForgeController> target;
    return target;
}

// A hidden <input type=file>; the chosen file comes back through the two functions below.
// Pictures the app cannot decode itself (HEIC from iPhones, WebP, AVIF ...) are turned into a
// JPEG by the browser first.
EM_JS(void, tf_pick_resume, (double maxBytes), {
    var input = document.createElement('input');
    input.type = 'file';
    input.accept = '.pdf,.docx,.odt,.rtf,.txt,.md,.markdown,.htm,.html,image/*,application/pdf,' +
                   'application/vnd.openxmlformats-officedocument.wordprocessingml.document,' +
                   'application/vnd.oasis.opendocument.text,application/rtf,text/plain,text/markdown,text/html';
    input.style.display = 'none';
    var give = function (name, blob) {
        blob.arrayBuffer().then(function (buffer) {
            Module.tfResumeFileLoaded(name, new Uint8Array(buffer));
        }).catch(function () {
            Module.tfResumeFileRejected(name, -1);
        });
    };
    input.onchange = function () {
        var file = input.files && input.files[0];
        input.remove();
        if (!file)
            return;
        if (file.size > maxBytes) {
            Module.tfResumeFileRejected(file.name, file.size);
            return;
        }
        // (No regular expressions or empty quotes here: this is C++ macro text until it is built.)
        var picture = file.type.indexOf('image/') === 0;
        var decodable = ['image/jpeg', 'image/png', 'image/bmp', 'image/gif'].indexOf(file.type) >= 0;
        if (picture && !decodable && window.createImageBitmap) {
            createImageBitmap(file).then(function (bitmap) {
                var canvas = document.createElement('canvas');
                canvas.width = bitmap.width;
                canvas.height = bitmap.height;
                canvas.getContext('2d').drawImage(bitmap, 0, 0);
                var dot = file.name.lastIndexOf('.');
                var base = dot > 0 ? file.name.substring(0, dot) : file.name;
                canvas.toBlob(function (jpeg) {
                    give(jpeg ? base + '.jpg' : file.name, jpeg || file);
                }, 'image/jpeg', 0.92);
            }).catch(function () {
                give(file.name, file);
            });
            return;
        }
        give(file.name, file);
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

// The picture goes to the page's TF_readDocument (web/static/app/app.js), which posts it to
// the server; the answer comes back through tfDocumentRead as JSON.
EM_JS(void, tf_read_document, (int job, const unsigned char *data, int length, int pdf), {
    var bytes = HEAPU8.slice(data, data + length);
    var done = function (answer) { Module.tfDocumentRead(job, JSON.stringify(answer || {})); };
    if (typeof window.TF_readDocument !== 'function') {
        done({ error: 'This page cannot send pictures to be read.' });
        return;
    }
    window.TF_readDocument(bytes, pdf ? 'application/pdf' : 'image/jpeg').then(done, function () {
        done({ error: 'The picture could not be sent. Try again.' });
    });
});

void documentReadInBrowser(int job, const std::string& json)
{
    if (TeamForgeController *target = pickerTarget().data())
        target->documentRead(job, QJsonDocument::fromJson(QByteArray::fromStdString(json)).toVariant().toMap());
}

EMSCRIPTEN_BINDINGS(teamforge_resume)
{
    emscripten::function("tfResumeFileLoaded", &resumeFileLoaded);
    emscripten::function("tfResumeFileRejected", &resumeFileRejected);
    emscripten::function("tfDocumentRead", &documentReadInBrowser);
}
#endif

// Whether the page told the app (TF_BOOT, src/web/WebBridge.cpp) that the server reads pictures.
bool browserReadsPictures()
{
#if defined(Q_OS_WASM)
    return qEnvironmentVariable("TEAMFORGE_READS_PICTURES") == QStringLiteral("1");
#else
    return false;
#endif
}

// The private copy's file name ending: the document's kind, or the image's own extension.
QString copyExtension(const QByteArray& bytes, const QString& fileName)
{
    try {
        switch (detectDocumentType(std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size())), ss(fileName))) {
        case DocumentType::Pdf: return QStringLiteral("pdf");
        case DocumentType::Docx: return QStringLiteral("docx");
        case DocumentType::Odt: return QStringLiteral("odt");
        case DocumentType::Rtf: return QStringLiteral("rtf");
        case DocumentType::Html: return QStringLiteral("html");
        case DocumentType::Text: return QStringLiteral("txt");
        case DocumentType::Image: break;
        }
    } catch (const DocumentError&) {
    }
    const QString ext = QFileInfo(fileName).suffix().toLower();
    static const QStringList images = {QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"),
                                       QStringLiteral("gif"), QStringLiteral("bmp"), QStringLiteral("webp"),
                                       QStringLiteral("tif"), QStringLiteral("tiff"), QStringLiteral("heic")};
    return images.contains(ext) ? ext : QStringLiteral("img");
}

// Existing private copies: resume-<id>.<any extension>.
QStringList resumeCopies(const QString& base)
{
    if (base.isEmpty())
        return {};
    const QFileInfo info(base);
    QStringList paths;
    for (const QFileInfo& file : QDir(info.absolutePath()).entryInfoList({info.fileName() + QStringLiteral(".*")}, QDir::Files))
        paths.append(file.absoluteFilePath());
    return paths;
}

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
    map.insert(QStringLiteral("readsPictures"), documentReader_ != nullptr || browserReadsPictures());
#if defined(Q_OS_WASM)
    map.insert(QStringLiteral("canKeepCopy"), false);
    map.insert(QStringLiteral("hasCopy"), false);
#else
    const QString path = resumeCopyPath();
    map.insert(QStringLiteral("canKeepCopy"), !resumeBytes_.isEmpty() && !path.isEmpty());
    map.insert(QStringLiteral("hasCopy"), !resumeCopies(path).isEmpty());
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

void TeamForgeController::setDocumentReader(DocumentReader reader, qint64 maxBytes)
{
    documentReader_ = std::move(reader);
    readMaxBytes_ = maxBytes > 0 ? maxBytes : kDefaultReadMaxBytes;
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
#if defined(Q_OS_WASM)
    if (!documentReader_ && browserReadsPictures()) {
        pickerTarget() = this;
        setDocumentReader([](int job, const QByteArray& picture, const QString& contentType) {
            tf_read_document(job, reinterpret_cast<const unsigned char *>(picture.constData()), static_cast<int>(picture.size()),
                             contentType == QStringLiteral("application/pdf") ? 1 : 0);
        }, qEnvironmentVariable("TEAMFORGE_READ_MAX_BYTES").toLongLong());
    }
#endif

    const int job = ++resumeJob_;
    resumeBytes_.clear();
    resumePicture_.clear();
    resumeFileName_ = fileName;
    resume_ = {{QStringLiteral("fileName"), fileName}, {QStringLiteral("sizeBytes"), bytes.size()},
               {QStringLiteral("sizeText"), sizeText(bytes.size())}};

    // Quick checks first, so a wrong file is reported at once.
    QString problem;
    DocumentType type = DocumentType::Pdf;
    if (bytes.isEmpty())
        problem = QStringLiteral("This file could not be read, or it is larger than %1 MB.").arg(resumeMaxMegabytes());
    else if (bytes.size() > qsizetype(resumeMaxMegabytes()) * 1024 * 1024)
        problem = QStringLiteral("This file is %1; resumes can be up to %2 MB.").arg(sizeText(bytes.size())).arg(resumeMaxMegabytes());
    else {
        try {
            type = detectDocumentType(std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size())), ss(fileName));
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
    const bool photo = type == DocumentType::Image;
    setResumeState(QStringLiteral("working"), 0.15, photo ? QStringLiteral("Opening the picture") : QStringLiteral("Reading the file"));

    // Everything the work needs is copied in: the worker never touches the controller's state.
    TeamForgeController *self = this;
    const qint64 readMax = readMaxBytes_ > 0 ? readMaxBytes_ : kDefaultReadMaxBytes;
    const bool readerSetUp = documentReader_ != nullptr;
    runResumeWork([self, job, photo, readMax, readerSetUp, data = bytes, name = ss(fileName), known = resumeVocabulary(),
                   catalog = skillCatalog_, today = QDate::currentDate()] {
        const auto stage = [self, job](double progress, const QString& text) {
            QMetaObject::invokeMethod(self, [self, job, progress, text] {
                if (self->resumeJob_ == job)
                    self->setResumeState(QStringLiteral("working"), progress, text);
            }, Qt::QueuedConnection);
        };
        QVariantMap result;
        QString error;
        QByteArray picture;
        QVariantMap info;
        QString reason;
        try {
            try {
                stage(0.4, photo ? QStringLiteral("Checking the picture is sharp enough to read") : QStringLiteral("Extracting the text"));
                const std::string text = documentText(std::string_view(data.constData(), static_cast<std::size_t>(data.size())), name);
                stage(0.75, QStringLiteral("Finding skills and experience"));
                result = analysisToMap(analyseText(text, known, today), catalog);
            } catch (const NeedsReadingError& needs) {
                reason = QString::fromUtf8(needs.what());
                if (photo) {
                    picture = preparePhoto(data, readMax, info);
                } else { // a scanned PDF: sent as it is
                    if (readerSetUp && data.size() > readMax)
                        throw DocumentError("This PDF is a scan (it has no text in it) of " + ss(sizeText(data.size()))
                                            + "; scans can be read up to " + std::to_string(readMax / (1024 * 1024))
                                            + " MB. Export it at a lower quality, or take a clear photo of each page.");
                    picture = data;
                    info = {{QStringLiteral("kind"), QStringLiteral("scan")}, {QStringLiteral("sizeText"), sizeText(data.size())}};
                }
            }
        } catch (const DocumentError& e) {
            error = QString::fromUtf8(e.what());
            picture.clear();
        } catch (const std::exception&) {
            error = QStringLiteral("This file could not be read. It may be damaged; try saving it again as PDF or DOCX.");
            picture.clear();
        }
        if (!picture.isEmpty())
            QMetaObject::invokeMethod(self, [self, job, picture, info, reason] { self->offerResumeReading(job, picture, info, reason); },
                                      Qt::QueuedConnection);
        else
            QMetaObject::invokeMethod(self, [self, job, result, error] { self->finishResume(job, result, error); },
                                      Qt::QueuedConnection);
    });
    return true;
}

void TeamForgeController::runResumeWork(std::function<void()> work)
{
#if QT_CONFIG(thread)
    resumeWorkers_.start(std::move(work));
#else
    // Single-threaded browser build: let the progress state paint, then work in place.
    QTimer::singleShot(30, this, std::move(work));
#endif
}

std::vector<std::string> TeamForgeController::resumeVocabulary() const
{
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
    return {known.begin(), known.end()};
}

void TeamForgeController::offerResumeReading(int job, const QByteArray& picture, const QVariantMap& info, const QString& reason)
{
    if (job != resumeJob_)
        return;
    const bool scan = info.value(QStringLiteral("kind")) == QStringLiteral("scan");
    if (!documentReader_) {
        resumeBytes_.clear();
#if defined(Q_OS_WASM)
        const QString where = QStringLiteral("Reading photos and scans is not turned on for this TeamForge site yet.");
#else
        const QString where = QStringLiteral("Photos and scans are read in the web version of TeamForge, which uses Azure AI to read the text.");
#endif
        resume_.insert(QStringLiteral("error"), reason + QLatin1Char(' ') + where
                                                    + QStringLiteral(" Here, upload your resume as a PDF, Word, ODT, RTF or text file, or add your details by hand."));
        setResumeState(QStringLiteral("error"), 0, QString());
        logEvent(AppLog::Level::Warning, QStringLiteral("resume"),
                 QStringLiteral("Resume %1 not read: no text reader here").arg(scan ? QStringLiteral("scan") : QStringLiteral("photo")));
        return;
    }
    resumePicture_ = picture;
    resume_.insert(QStringLiteral("picture"), info);
    resume_.insert(QStringLiteral("reason"), reason);
    setResumeState(QStringLiteral("confirm"), 0.5, scan ? QStringLiteral("This is a scan") : QStringLiteral("The picture looks sharp enough"));
}

bool TeamForgeController::confirmResumeReading()
{
    return run("confirmResumeReading", QString(), [&] {
        require(Permission::EditOwnProfile);
        if (resume_.value(QStringLiteral("state")) != QStringLiteral("confirm") || resumePicture_.isEmpty() || !documentReader_)
            throw ValidationError("There is no picture waiting to be read");
        const bool scan = resume_.value(QStringLiteral("picture")).toMap().value(QStringLiteral("kind")) == QStringLiteral("scan");
        setResumeState(QStringLiteral("working"), 0.6, QStringLiteral("Reading the text (Azure AI)"));
        logEvent(AppLog::Level::Info, QStringLiteral("resume"),
                 QStringLiteral("Resume %1 sent to be read (%2)").arg(scan ? QStringLiteral("scan") : QStringLiteral("photo"), sizeText(resumePicture_.size())));
        const QByteArray picture = resumePicture_;
        resumePicture_.clear();
        documentReader_(resumeJob_, picture, scan ? QStringLiteral("application/pdf") : QStringLiteral("image/jpeg"));
    });
}

void TeamForgeController::documentRead(int job, const QVariantMap& answer)
{
    if (job != resumeJob_ || resume_.value(QStringLiteral("state")) != QStringLiteral("working"))
        return; // cancelled or replaced meanwhile
    const QString problem = answer.value(QStringLiteral("error")).toString();
    if (!answer.contains(QStringLiteral("text")) || !problem.isEmpty()) {
        finishResume(job, {}, problem.isEmpty() ? QStringLiteral("The picture could not be read. Try again.") : problem);
        return;
    }
    ReadText read;
    read.text = ss(answer.value(QStringLiteral("text")).toString());
    read.pages = answer.value(QStringLiteral("pages")).toInt();
    for (const QVariant& confidence : answer.value(QStringLiteral("confidences")).toList())
        read.wordConfidence.push_back(std::clamp(confidence.toDouble(), 0.0, 1.0));
    setResumeState(QStringLiteral("working"), 0.85, QStringLiteral("Finding skills and experience"));
    TeamForgeController *self = this;
    runResumeWork([self, job, read, known = resumeVocabulary(), catalog = skillCatalog_, today = QDate::currentDate()] {
        QVariantMap result;
        QString error;
        try {
            result = analysisToMap(analyseText(acceptReadText(read), known, today), catalog);
        } catch (const DocumentError& e) {
            error = QString::fromUtf8(e.what());
        } catch (const std::exception&) {
            error = QStringLiteral("The text that was read could not be analysed. Try again, or add your details by hand.");
        }
        QMetaObject::invokeMethod(self, [self, job, result, error] { self->finishResume(job, result, error); },
                                  Qt::QueuedConnection);
    });
}

void TeamForgeController::finishResume(int job, const QVariantMap& result, const QString& error)
{
    if (job != resumeJob_)
        return; // cleared or replaced meanwhile
    resume_.remove(QStringLiteral("picture"));
    resume_.remove(QStringLiteral("reason"));
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
    resumePicture_.clear();
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
        const QString path = base + QLatin1Char('.') + copyExtension(resumeBytes_, resumeFileName_);
        for (const QString& old : resumeCopies(base))
            if (old != QFileInfo(path).absoluteFilePath())
                QFile::remove(old);
        QSaveFile out(path);
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
        for (const QString& path : resumeCopies(resumeCopyPath()))
            QFile::remove(path);
        logEvent(AppLog::Level::Info, QStringLiteral("resume"), QStringLiteral("Private resume copy deleted"));
        emit resumeChanged();
    });
}
