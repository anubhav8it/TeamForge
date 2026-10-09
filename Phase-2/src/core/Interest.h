#pragma once

#include "core/Repository.h"

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace teamforge {

// Where a participant's interest in a project stands. A participant expresses interest; the
// host reviews it and accepts or declines. Accepting does not put anyone in a team: the host
// still decides the team, and accepted participants are simply the ones cleared to join it.
enum class InterestStatus { Interested, UnderReview, Accepted, Declined };

// "interested", "under_review", "accepted", "declined" (the stored spelling).
std::string interestStatusName(InterestStatus status);
// Throws ValidationError on an unknown status.
InterestStatus interestStatusFrom(std::string_view text);

class InterestRequest
{
public:
    // Throws ValidationError on an empty id, participant or project.
    InterestRequest(const std::string& id, const std::string& studentId, const std::string& requirementId,
                    InterestStatus status = InterestStatus::Interested);

    const std::string& id() const { return id_; }
    const std::string& studentId() const { return studentId_; }
    const std::string& requirementId() const { return requirementId_; }
    InterestStatus status() const { return status_; }

    // Interested -> UnderReview, Accepted or Declined; UnderReview -> Accepted or Declined;
    // Accepted <-> Declined (the host changes their mind). Nothing goes back to Interested.
    static bool canMove(InterestStatus from, InterestStatus to);
    // Throws ValidationError for a move canMove() rejects.
    void moveTo(InterestStatus next);

private:
    std::string id_;
    std::string studentId_;
    std::string requirementId_;
    InterestStatus status_;
};

// Source of the engagement factor. Abstract, so the scoring strategies do not depend on how or
// where interest is stored (and tests can supply their own).
class EngagementSource
{
public:
    virtual ~EngagementSource() = default;
    // Engagement of `studentId` with `requirementId`, in [0, 1].
    virtual double engagement(const std::string& studentId, const std::string& requirementId) const = 0;
};

// No interest expressed yet: neither a penalty nor a reward.
constexpr double kNeutralEngagement = 0.5;
// The participant has expressed interest in the project (whatever the host decided since).
constexpr double kInterestedEngagement = 1.0;

// All interest requests. Stored by id (Repository, a balanced tree) with a second index on
// (participant, project), so a participant has at most one request per project and the
// engagement lookup is O(log n).
class InterestBook final : public EngagementSource
{
public:
    // A new request in Interested. DuplicateError if the participant already expressed interest
    // in that project.
    const InterestRequest& express(const std::string& studentId, const std::string& requirementId);
    // NotFoundError for an unknown request; ValidationError for a move canMove() rejects.
    const InterestRequest& setStatus(const std::string& requestId, InterestStatus status);
    // For loading. DuplicateError on a taken id or a second request for the same pair.
    void add(const InterestRequest& request);

    const InterestRequest& get(const std::string& requestId) const { return requests_.get(requestId); }
    const InterestRequest *find(const std::string& studentId, const std::string& requirementId) const;
    std::vector<InterestRequest> forRequirement(const std::string& requirementId) const;
    std::vector<InterestRequest> forStudent(const std::string& studentId) const;
    std::vector<InterestRequest> values() const { return requests_.values(); }
    std::size_t size() const { return requests_.size(); }

    // Drop the requests of a removed participant or project.
    void removeStudent(const std::string& studentId);
    void removeRequirement(const std::string& requirementId);

    double engagement(const std::string& studentId, const std::string& requirementId) const override;

private:
    std::string nextId() const;

    Repository<InterestRequest> requests_{"Interest request"};
    std::map<std::pair<std::string, std::string>, std::string> byPair_;
};

} // namespace teamforge
