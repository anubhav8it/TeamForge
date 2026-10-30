#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace teamforge {

// Plain text from an uploaded document, for the resume analyser. Standard C++ only: DOCX is a
// ZIP of XML parts, PDF a set of objects whose page content streams draw the text.
//
// The result is UTF-8, one line per text line (PDF) or paragraph (DOCX). Throws DocumentError,
// with a message meant for the person who uploaded the file, for: an unsupported or unknown
// type, a damaged file, an encrypted (password-protected) PDF, an old binary .doc, or a
// document with no extractable text (a scanned image).

enum class DocumentType { Pdf, Docx };

// From the file's first bytes; the file name's extension is only a fallback hint. Throws
// DocumentError when the content is neither a PDF nor a DOCX.
DocumentType detectDocumentType(std::string_view bytes, std::string_view fileName);

std::string docxText(std::string_view bytes);
std::string pdfText(std::string_view bytes);
std::string documentText(std::string_view bytes, std::string_view fileName);

} // namespace teamforge
