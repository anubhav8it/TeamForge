cmake_minimum_required(VERSION 3.16)
project(TeamForge LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(TP ${CMAKE_SOURCE_DIR}/third_party)
foreach(f httplib.h sqlite3.c sqlite3.h)
  if(NOT EXISTS ${TP}/${f})
    message(FATAL_ERROR "third_party/${f} is missing. Run third_party/get-deps.ps1 (Windows) "
                        "or third_party/get-deps.sh (Linux/macOS) first. See README.md.")
  endif()
endforeach()

find_package(Threads REQUIRED)

# SQLite, compiled from the official single-file "amalgamation"
add_library(sqlite3 STATIC ${TP}/sqlite3.c)
target_include_directories(sqlite3 PUBLIC ${TP})
target_compile_definitions(sqlite3 PRIVATE SQLITE_THREADSAFE=1 SQLITE_OMIT_LOAD_EXTENSION SQLITE_DQS=0)
target_link_libraries(sqlite3 PUBLIC Threads::Threads ${CMAKE_DL_LIBS})

# Everything except HTTP: JSON, crypto, database, matching, API routes
add_library(teamforge_core STATIC
  src/api.cpp src/db.cpp src/json.cpp src/crypto.cpp src/matching.cpp)
target_link_libraries(teamforge_core PUBLIC sqlite3)

if(MSVC)
  target_compile_options(teamforge_core PRIVATE /W4 /utf-8)
else()
  target_compile_options(teamforge_core PRIVATE -Wall -Wextra)
endif()

# The server (cpp-httplib)
add_executable(teamforge src/main.cpp)
target_include_directories(teamforge PRIVATE ${TP})
target_link_libraries(teamforge PRIVATE teamforge_core Threads::Threads)
if(WIN32)
  # cpp-httplib needs Winsock and Windows 10+ APIs
  target_compile_definitions(teamforge PRIVATE _WIN32_WINNT=0x0A00)
  target_link_libraries(teamforge PRIVATE ws2_32)
endif()
if(MINGW)
  # One self-contained .exe: no libstdc++/libgcc/winpthread DLLs to ship or mismatch
  target_link_options(teamforge PRIVATE -static)
endif()

# Copy the website and sample data next to the program so it runs from the build folder too
add_custom_command(TARGET teamforge POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/static $<TARGET_FILE_DIR:teamforge>/static
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/data $<TARGET_FILE_DIR:teamforge>/data)

# Tests:  ctest --test-dir build --output-on-failure
enable_testing()
add_executable(teamforge_core_tests tests/test_core.cpp src/json.cpp src/crypto.cpp src/matching.cpp)
add_executable(teamforge_api_tests tests/test_api.cpp)
target_link_libraries(teamforge_api_tests PRIVATE teamforge_core)
add_test(NAME core COMMAND teamforge_core_tests)
add_test(NAME api COMMAND teamforge_api_tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
