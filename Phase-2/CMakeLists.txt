cmake_minimum_required(VERSION 3.22)

project(TeamForge VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Quick)
qt_standard_project_setup(REQUIRES 6.8)

# Project warning flags, linked privately into each TeamForge target (not Qt-generated code).
add_library(teamforge_warnings INTERFACE)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(teamforge_warnings INTERFACE -Wall -Wextra -Wpedantic)
endif()

# Matching engine, data structures and persistence; no UI code.
add_subdirectory(src/core)

# The UI: the QML module "TeamForge" plus the C++ bridge (src/app), exposed to QML as the
# singleton Backend. A static library, so the app and the UI test (tests/tst_ui) load the same
# module.
qt_add_library(teamforge_ui STATIC)

# The browser build (Qt for WebAssembly) also bundles a monospace font, because a web page has no
# system fonts. Desktop builds are unchanged.
set(TEAMFORGE_WEB_RESOURCES "")
if(EMSCRIPTEN)
    set(TEAMFORGE_WEB_RESOURCES assets/fonts/NotoSansMono-Regular.ttf)
endif()

set_source_files_properties(ui/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(teamforge_ui
    URI TeamForge
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/TeamForge
    QML_FILES
        ui/Main.qml
        ui/Theme.qml
        ui/components/Avatar.qml
        ui/components/Chip.qml
        ui/components/CheckResult.qml
        ui/components/ConfirmDialog.qml
        ui/components/CoverageRing.qml
        ui/components/EmptyState.qml
        ui/components/ErrorBanner.qml
        ui/components/LevelPips.qml
        ui/components/LogoMark3D.qml
        ui/components/MeterBar.qml
        ui/components/NavItem.qml
        ui/components/Panel.qml
        ui/components/Segmented.qml
        ui/components/Sidebar.qml
        ui/components/SkillChips.qml
        ui/components/SkillField.qml
        ui/components/StatTile.qml
        ui/components/TfButton.qml
        ui/components/TfComboBox.qml
        ui/components/TfPage.qml
        ui/components/TfSpinBox.qml
        ui/components/TfText.qml
        ui/components/TfTextField.qml
        ui/components/Toast.qml
        ui/components/TopBar.qml
        ui/pages/EntryScreen.qml
        ui/pages/MatchInsightsDrawer.qml
        ui/host/HostWorkspace.qml
        ui/host/ApplicantsPage.qml
        ui/host/InterestReviewList.qml
        ui/host/OverviewPage.qml
        ui/host/MatchingPage.qml
        ui/host/ParticipantsPage.qml
        ui/host/ProjectsPage.qml
        ui/host/TeamBuilderPanel.qml
        ui/participant/ParticipantWorkspace.qml
        ui/participant/ParticipantHome.qml
        ui/participant/MyProfilePage.qml
        ui/participant/OpportunitiesPage.qml
        ui/participant/OpportunityCard.qml
        ui/participant/OpportunityDrawer.qml
        ui/participant/ProfileEditor.qml
        ui/participant/ProfilePicker.qml
        ui/developer/DeveloperWorkspace.qml
        ui/developer/DevOverviewPage.qml
        ui/developer/DevParticipantsPage.qml
        ui/developer/DevSkillsPage.qml
        ui/developer/DevWorkspacePage.qml
        ui/developer/RecordInspector.qml
    RESOURCES
        assets/teamforge_logo_dark.png
        assets/teamforge_mark_dark.png
        assets/teamforge_mark_light.png
        assets/teamforge_mark_red.png
        assets/teamforge_mark_depth.png
        assets/fonts/Manrope-Regular.ttf
        assets/fonts/Manrope-Medium.ttf
        assets/fonts/Manrope-SemiBold.ttf
        assets/fonts/Manrope-Bold.ttf
        assets/fonts/Manrope-ExtraBold.ttf
        # Committed sample data, bundled so the app can seed and reset its runtime data folder
        # (src/app/SampleData.cpp). Rebuilding embeds the current samples.
        data/students.json
        data/requirements.json
        data/teams.json
        data/skills.json
        data/interest_requests.json
        ${TEAMFORGE_WEB_RESOURCES}
    SOURCES
        src/app/TeamForgeController.cpp src/app/TeamForgeController.h
        src/app/TeamForgeParticipant.cpp src/app/TeamForgeDeveloper.cpp src/app/TeamForgeRecords.cpp src/app/TeamForgeInterest.cpp
        src/app/BridgeSupport.cpp src/app/BridgeSupport.h
        src/app/AppLog.cpp src/app/AppLog.h
        src/app/SampleData.cpp src/app/SampleData.h
)

target_include_directories(teamforge_ui PUBLIC src/app)
target_link_libraries(teamforge_ui PUBLIC Qt6::Quick teamforge_core PRIVATE teamforge_warnings)

# The application: main.cpp only; everything else is in teamforge_ui.
qt_add_executable(teamforge
    src/main.cpp
)

set_target_properties(teamforge PROPERTIES
    OUTPUT_NAME TeamForge
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(teamforge PRIVATE teamforge_ui teamforge_uiplugin teamforge_warnings)
target_compile_definitions(teamforge PRIVATE TEAMFORGE_VERSION="${PROJECT_VERSION}")

# Browser build: the bridge to the TeamForge web page (src/web/WebBridge.h), which loads the
# shared data from the server before the app starts and uploads every saved change.
if(EMSCRIPTEN)
    target_sources(teamforge PRIVATE src/web/WebBridge.cpp src/web/WebBridge.h)
    target_include_directories(teamforge PRIVATE src/web)
    # WebBridge exports tfFlush() to the page with Embind (Qt links it too; repeating is harmless).
    target_link_options(teamforge PRIVATE "-lembind")
endif()

# The app reads and saves <exe dir>/data (override with TEAMFORGE_DATA_DIR), never data/ itself.
# On startup it syncs that folder with the bundled samples (src/app/SampleData.h): unchanged
# samples leave saved work alone; changed samples replace it, backing up the old files. This
# works for any build folder (including Qt Creator's) without a build step.

# The tests are native executables; the browser build skips them (run them in a desktop build).
if(EMSCRIPTEN)
    set(TEAMFORGE_BUILD_TESTS OFF CACHE BOOL "Build the TeamForge unit tests" FORCE)
endif()
option(TEAMFORGE_BUILD_TESTS "Build the TeamForge unit tests" ON)
if(TEAMFORGE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
