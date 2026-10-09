#pragma once

#include <stdexcept>

namespace teamforge {

// Root of every exception the core throws, so callers (UI, CLI) can catch one type.
class TeamForgeError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

// Input breaks a domain rule: empty name, level outside 1..5, bad time slot, weights not summing to 1.
class ValidationError : public TeamForgeError
{
public:
    using TeamForgeError::TeamForgeError;
};

// Lookup of an id that does not exist.
class NotFoundError : public TeamForgeError
{
public:
    using TeamForgeError::TeamForgeError;
};

// An id or team member that already exists was added again.
class DuplicateError : public TeamForgeError
{
public:
    using TeamForgeError::TeamForgeError;
};

// A team operation would violate the requirement's size limits.
class TeamConstraintError : public TeamForgeError
{
public:
    using TeamForgeError::TeamForgeError;
};

// Reading or writing a data file failed (I/O, malformed JSON, invalid record).
class PersistenceError : public TeamForgeError
{
public:
    using TeamForgeError::TeamForgeError;
};

} // namespace teamforge
