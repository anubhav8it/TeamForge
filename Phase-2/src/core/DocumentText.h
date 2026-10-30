#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace teamforge {

// Plain text from an uploaded document, for the resume analyser. Standard C++ only: DOCX and ODT
// are ZIPs of XML parts, PDF a set of objects whose page content streams draw the text, RTF a
// marked-up text stream; text, Markdown and HTML files are decoded and stripped of markup.
//
// The result is UTF-8, one line per text line (PDF) or paragraph. Throws DocumentError, with a
// message meant for the person who uploaded the file, for: an unknown type, a damaged file, an
// encrypted (password-protected) PDF, an old binary .doc, or a file with too little text.
// Photos and scanned PDFs throw NeedsReadingError: their text needs OCR (acceptReadText).

enum class DocumentType { Pdf, Docx, Odt, Rtf, Text, Html, Image };

// From the file's content; the file name's extension is only a hint (Markdown, a damaged file).
// Throws DocumentError when the content is none of the types above.
DocumentType detectDocumentType(std::string_view bytes, std::string_view fileName);

std::string docxText(std::string_view bytes);
std::string odtText(std::string_view bytes);
std::string pdfText(std::string_view bytes);
std::string rtfText(std::string_view bytes);
std::string htmlText(std::string_view bytes);
// `markdown`: also drops heading marks and emphasis (# Skills, **Python**).
std::string plainText(std::string_view bytes, bool markdown = false);
std::string documentText(std::string_view bytes, std::string_view fileName);

// Text read by OCR from a photo or scan, with the reader's confidence (0..1) in each word.
struct ReadText
{
    std::string text;
    std::vector<double> wordConfidence;
    int pages = 0;
};

// The OCR text, tidied like the other formats. Throws DocumentError when it is too short, or
// when too many words were uncertain: the photo is blurred, too small or unevenly lit, and
// what was read cannot be trusted.
std::string acceptReadText(const ReadText& read);

} // namespace teamforge
