add_library(teamforge_core STATIC
    CoverageOnlyStrategy.cpp CoverageOnlyStrategy.h
    Errors.h
    Interest.cpp Interest.h
    JsonStorage.cpp JsonStorage.h
    MatchingEngine.cpp MatchingEngine.h
    MatchingStrategy.cpp MatchingStrategy.h
    MatchResult.cpp MatchResult.h
    ProjectRequirement.cpp ProjectRequirement.h
    Ranker.cpp Ranker.h
    Repository.h
    Skill.cpp Skill.h
    SkillBST.cpp SkillBST.h
    SkillGraph.cpp SkillGraph.h
    SkillIndex.cpp SkillIndex.h
    Student.cpp Student.h
    Team.cpp Team.h
    Validation.cpp Validation.h
    WeightedMatchingStrategy.cpp WeightedMatchingStrategy.h
)

# Headers are included as "core/Xyz.h".
target_include_directories(teamforge_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
# Qt Core is used only by JsonStorage.cpp; no core header includes Qt.
target_link_libraries(teamforge_core PRIVATE Qt6::Core teamforge_warnings)
