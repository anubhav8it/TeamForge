#include "core/Validation.h"

#include "core/Errors.h"

#include <cctype>

namespace teamforge {

std::string trimmed(std::string_view text)
{
    auto isSpace = [](char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; };
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && isSpace(text[begin]))
        ++begin;
    while (end > begin && isSpace(text[end - 1]))
        --end;
    return std::string(text.substr(begin, end - begin));
}

std::string requireNonEmpty(std::string_view value, std::string_view field)
{
    std::string result = trimmed(value);
    if (result.empty())
        throw ValidationError(std::string(field) + " must not be empty");
    return result;
}

} // namespace teamforge
