#include "core/Interest.h"

#include "core/Errors.h"
#include "core/Validation.h"

#include <cstdio>

namespace teamforge {

std::string interestStatusName(InterestStatus status)
{
    switch (status) {
    case InterestStatus::Interested: return "interested";
    case InterestStatus::UnderReview: return "under_review";
    case InterestStatus::Accepted: return "accepted";
    case InterestStatus::Declined: return "declined";
    }
    return "interested";
}

InterestStatus interestStatusFrom(std::string_view text)
{
    if (text == "interested") return InterestStatus::Interested;
    if (text == "under_review") return InterestStatus::UnderReview;
    if (text == "accepted") return InterestStatus::Accepted;
    if (text == "declined") return InterestStatus::Declined;
    throw ValidationError("Unknown interest status '" + std::string(text) + "'");
}

InterestRequest::InterestRequest(const std::string& id, const std::string& studentId,
                                 const std::string& requirementId, InterestStatus status)
    : id_(requireNonEmpty(id, "Interest request id"))
    , studentId_(requireNonEmpty(studentId, "Interest request participant"))
    , requirementId_(requireNonEmpty(requirementId, "Interest request project"))
    , status_(status)
{
}

bool InterestRequest::canMove(InterestStatus from, InterestStatus to)
{
    switch (from) {
    case InterestStatus::Interested: return to != InterestStatus::Interested;
    case InterestStatus::UnderReview: return to == InterestStatus::Accepted || to == InterestStatus::Declined;
    case InterestStatus::Accepted: return to == InterestStatus::Declined;
    case InterestStatus::Declined: return to == InterestStatus::Accepted;
    }
    return false;
}

void InterestRequest::moveTo(InterestStatus next)
{
    if (!canMove(status_, next))
        throw ValidationError("Interest request '" + id_ + "' cannot move from " + interestStatusName(status_) + " to "
                              + interestStatusName(next));
    status_ = next;
}

const InterestRequest& InterestBook::express(const std::string& studentId, const std::string& requirementId)
{
    InterestRequest request(nextId(), studentId, requirementId);
    add(request);
    return requests_.get(request.id());
}

const InterestRequest& InterestBook::setStatus(const std::string& requestId, InterestStatus status)
{
    InterestRequest request = requests_.get(requestId);
    request.moveTo(status);
    requests_.update(request);
    return requests_.get(requestId);
}

void InterestBook::add(const InterestRequest& request)
{
    const auto key = std::make_pair(request.studentId(), request.requirementId());
    if (byPair_.count(key) != 0)
        throw DuplicateError("Participant '" + request.studentId() + "' has already expressed interest in '"
                             + request.requirementId() + "'");
    requests_.add(request); // DuplicateError on a taken id
    byPair_.emplace(key, request.id());
}

const InterestRequest *InterestBook::find(const std::string& studentId, const std::string& requirementId) const
{
    const auto it = byPair_.find(std::make_pair(studentId, requirementId));
    return it == byPair_.end() ? nullptr : requests_.find(it->second);
}

std::vector<InterestRequest> InterestBook::forRequirement(const std::string& requirementId) const
{
    return requests_.filter([&](const InterestRequest& r) { return r.requirementId() == requirementId; });
}

std::vector<InterestRequest> InterestBook::forStudent(const std::string& studentId) const
{
    return requests_.filter([&](const InterestRequest& r) { return r.studentId() == studentId; });
}

void InterestBook::removeStudent(const std::string& studentId)
{
    for (const InterestRequest& request : forStudent(studentId)) {
        byPair_.erase(std::make_pair(request.studentId(), request.requirementId()));
        requests_.remove(request.id());
    }
}

void InterestBook::removeRequirement(const std::string& requirementId)
{
    for (const InterestRequest& request : forRequirement(requirementId)) {
        byPair_.erase(std::make_pair(request.studentId(), request.requirementId()));
        requests_.remove(request.id());
    }
}

double InterestBook::engagement(const std::string& studentId, const std::string& requirementId) const
{
    return find(studentId, requirementId) != nullptr ? kInterestedEngagement : kNeutralEngagement;
}

std::string InterestBook::nextId() const
{
    // "int-0001", "int-0002", ...: the first free number, so ids stay short and deterministic.
    for (std::size_t n = requests_.size() + 1;; ++n) {
        char buffer[24];
        std::snprintf(buffer, sizeof buffer, "int-%04zu", n);
        if (!requests_.contains(buffer))
            return buffer;
    }
}

} // namespace teamforge
