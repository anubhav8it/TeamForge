#pragma once

#include <string>
#include <string_view>

namespace teamforge {

std::string trimmed(std::string_view text);

// Returns `value` trimmed; throws ValidationError naming `field` if nothing is left.
std::string requireNonEmpty(std::string_view value, std::string_view field);

} // namespace teamforge
