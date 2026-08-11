# Shared version data for every standalone CMake entry point.
# project(VERSION) only accepts up to four dot-separated integers, so keep the
# numeric release and the Git-qualified build version as separate variables.
set(CONVAI_PROJECT_VERSION_BASE "26.8.0")

get_filename_component(_CONVAI_REPOSITORY_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

execute_process(
    COMMAND git rev-list --all --count
    WORKING_DIRECTORY "${_CONVAI_REPOSITORY_ROOT}"
    RESULT_VARIABLE _CONVAI_GIT_COUNT_RESULT
    OUTPUT_VARIABLE CONVAI_GIT_COMMIT_COUNT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY "${_CONVAI_REPOSITORY_ROOT}"
    RESULT_VARIABLE _CONVAI_GIT_HASH_RESULT
    OUTPUT_VARIABLE CONVAI_GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(_CONVAI_GIT_COUNT_RESULT STREQUAL "0" AND
   _CONVAI_GIT_HASH_RESULT STREQUAL "0" AND
   NOT CONVAI_GIT_COMMIT_COUNT STREQUAL "" AND
   NOT CONVAI_GIT_COMMIT_HASH STREQUAL "")
    set(CONVAI_PROJECT_VERSION
        "${CONVAI_PROJECT_VERSION_BASE}.${CONVAI_GIT_COMMIT_COUNT}-${CONVAI_GIT_COMMIT_HASH}")
else()
    # Source archives without Git metadata still receive a valid release
    # version instead of an incomplete '.-' suffix.
    set(CONVAI_PROJECT_VERSION "${CONVAI_PROJECT_VERSION_BASE}")
    set(CONVAI_GIT_COMMIT_COUNT "")
    set(CONVAI_GIT_COMMIT_HASH "")
endif()

unset(_CONVAI_REPOSITORY_ROOT)
unset(_CONVAI_GIT_COUNT_RESULT)
unset(_CONVAI_GIT_HASH_RESULT)
