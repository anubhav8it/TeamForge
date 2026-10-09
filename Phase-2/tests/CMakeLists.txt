find_package(Qt6 REQUIRED COMPONENTS Test)

# teamforge_add_test(<name> <sources...>)
# Builds a Qt Test executable against teamforge_core and registers it with CTest. Qt's bin
# directory is prepended to PATH so the test finds the Qt DLLs without a deploy step.
function(teamforge_add_test name)
    qt_add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE teamforge_core teamforge_warnings Qt6::Test)
    target_compile_definitions(${name} PRIVATE TEAMFORGE_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    )
endfunction()

teamforge_add_test(tst_domain tst_domain.cpp TestSupport.h)
teamforge_add_test(tst_scoring tst_scoring.cpp TestSupport.h)
teamforge_add_test(tst_dsa tst_dsa.cpp TestSupport.h)
teamforge_add_test(tst_engine tst_engine.cpp TestSupport.h)
teamforge_add_test(tst_persistence tst_persistence.cpp TestSupport.h)

# The QML bridge is compiled into the app's QML module; the test builds the same sources directly.
teamforge_add_test(tst_controller tst_controller.cpp
    ${PROJECT_SOURCE_DIR}/src/app/TeamForgeController.cpp
    ${PROJECT_SOURCE_DIR}/src/app/TeamForgeController.h
    ${PROJECT_SOURCE_DIR}/src/app/TeamForgeParticipant.cpp
    ${PROJECT_SOURCE_DIR}/src/app/TeamForgeDeveloper.cpp
    ${PROJECT_SOURCE_DIR}/src/app/TeamForgeRecords.cpp
    ${PROJECT_SOURCE_DIR}/src/app/TeamForgeInterest.cpp
    ${PROJECT_SOURCE_DIR}/src/app/BridgeSupport.cpp
    ${PROJECT_SOURCE_DIR}/src/app/AppLog.cpp
    ${PROJECT_SOURCE_DIR}/src/app/SampleData.cpp
)
target_include_directories(tst_controller PRIVATE ${PROJECT_SOURCE_DIR}/src/app)
target_link_libraries(tst_controller PRIVATE Qt6::Qml)

# The real UI, loaded offscreen: workspaces, permissions in the UI, hover and tooltips.
teamforge_add_test(tst_ui tst_ui.cpp)
target_link_libraries(tst_ui PRIVATE teamforge_ui teamforge_uiplugin)
set_tests_properties(tst_ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
