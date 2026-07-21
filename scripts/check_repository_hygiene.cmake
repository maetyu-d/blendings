cmake_minimum_required(VERSION 3.22)

get_filename_component(REPOSITORY_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
find_package(Git REQUIRED QUIET)

execute_process(
    COMMAND "${GIT_EXECUTABLE}" -C "${REPOSITORY_ROOT}" ls-files
    RESULT_VARIABLE GIT_RESULT
    OUTPUT_VARIABLE TRACKED_OUTPUT
    ERROR_VARIABLE GIT_ERROR
    OUTPUT_STRIP_TRAILING_WHITESPACE)

if (NOT GIT_RESULT EQUAL 0)
    message(FATAL_ERROR "Could not inspect tracked files: ${GIT_ERROR}")
endif()

string(REPLACE "\n" ";" TRACKED_FILES "${TRACKED_OUTPUT}")
set(CANONICAL_DIRECTORIES Source Tests scripts Demos docs third_party)
set(PROBLEMS)

foreach (TRACKED_FILE IN LISTS TRACKED_FILES)
    if (TRACKED_FILE STREQUAL "")
        continue()
    endif()

    string(REGEX MATCH "^[^/]+" TOP_LEVEL_DIRECTORY "${TRACKED_FILE}")
    string(TOLOWER "${TOP_LEVEL_DIRECTORY}" TOP_LEVEL_LOWER)
    foreach (CANONICAL_DIRECTORY IN LISTS CANONICAL_DIRECTORIES)
        string(TOLOWER "${CANONICAL_DIRECTORY}" CANONICAL_LOWER)
        if (TOP_LEVEL_LOWER STREQUAL CANONICAL_LOWER
            AND NOT TOP_LEVEL_DIRECTORY STREQUAL CANONICAL_DIRECTORY)
            list(APPEND PROBLEMS
                "${TRACKED_FILE}: top-level directory must be '${CANONICAL_DIRECTORY}'")
        endif()
    endforeach()

    if (TRACKED_FILE MATCHES "(^|/)(build[^/]*|cmake-build[^/]*|CMakeFiles|DerivedData|dist|release)/"
        OR TRACKED_FILE MATCHES "\\.(app|dmg|zip|tar\\.gz|o|obj|a|dylib|so|dll)$"
        OR TRACKED_FILE MATCHES "(^|/)CMakeCache\\.txt$")
        list(APPEND PROBLEMS "${TRACKED_FILE}: generated output must not be tracked")
    endif()

    if (TRACKED_FILE MATCHES "(^|/)[^/]*Smoke\\.cpp$"
        AND NOT TRACKED_FILE MATCHES "^Tests/")
        list(APPEND PROBLEMS "${TRACKED_FILE}: smoke-test sources belong in Tests/")
    endif()
endforeach()

if (PROBLEMS)
    list(JOIN PROBLEMS "\n  - " PROBLEM_REPORT)
    message(FATAL_ERROR "Repository hygiene check failed:\n  - ${PROBLEM_REPORT}")
endif()

message(STATUS "Repository hygiene check passed (${REPOSITORY_ROOT})")
