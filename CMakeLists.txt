cmake_minimum_required(VERSION 3.27)

project(msms-manager
    VERSION 1.0.0
    DESCRIPTION "Offline desktop application for managing MSMS membership and society records"
    HOMEPAGE_URL "https://github.com/msms/msms-manager"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_VERBOSE_LOGGING "Enable verbose logging in debug mode" OFF)

include(FetchContent)

# ---------------------------------------------------------------------------
# wxWidgets 3.2 Configuration with Fallback
# ---------------------------------------------------------------------------
find_package(wxWidgets 3.2 COMPONENTS core base adv QUIET)
if(NOT wxWidgets_FOUND)
    find_package(wxWidgets COMPONENTS Core Base Adv QUIET)
    if(NOT wxWidgets_FOUND)
        find_package(wxWidgets COMPONENTS core base QUIET)
    endif()
endif()

if(wxWidgets_FOUND)
    message(STATUS "Found host wxWidgets ${wxWidgets_VERSION_STRING}: ${wxWidgets_LIBRARIES}")

    # The legacy FindwxWidgets module exports variables rather than modern targets.
    if(NOT TARGET wxWidgets::Base)
        add_library(wxWidgets::Base INTERFACE IMPORTED GLOBAL)
        if(wxWidgets_INCLUDE_DIRS)
            target_include_directories(wxWidgets::Base INTERFACE ${wxWidgets_INCLUDE_DIRS})
        endif()
        if(wxWidgets_DEFINITIONS)
            target_compile_definitions(wxWidgets::Base INTERFACE ${wxWidgets_DEFINITIONS})
        endif()
        if(wxWidgets_LIBRARIES)
            target_link_libraries(wxWidgets::Base INTERFACE ${wxWidgets_LIBRARIES})
        endif()
    endif()
    if(NOT TARGET wxWidgets::Core)
        add_library(wxWidgets::Core INTERFACE IMPORTED GLOBAL)
        target_link_libraries(wxWidgets::Core INTERFACE wxWidgets::Base)
    endif()
    if(NOT TARGET wxWidgets::Adv)
        add_library(wxWidgets::Adv INTERFACE IMPORTED GLOBAL)
        target_link_libraries(wxWidgets::Adv INTERFACE wxWidgets::Base)
    endif()
else()
    message(STATUS "wxWidgets not found on host system. Falling back to FetchContent...")
    FetchContent_Declare(
        wxWidgets
        GIT_REPOSITORY https://github.com/wxWidgets/wxWidgets.git
        GIT_TAG        v3.2.5
        GIT_SHALLOW    TRUE
    )
    set(wxBUILD_SHARED OFF CACHE BOOL "Build wxWidgets as static libraries" FORCE)
    set(wxBUILD_TESTS OFF CACHE BOOL "Disable wxWidgets tests" FORCE)
    set(wxBUILD_SAMPLES OFF CACHE BOOL "Disable wxWidgets samples" FORCE)
    set(wxBUILD_DEMOS OFF CACHE BOOL "Disable wxWidgets demos" FORCE)
    set(wxBUILD_INSTALL OFF CACHE BOOL "Disable wxWidgets installation targets" FORCE)
    FetchContent_MakeAvailable(wxWidgets)

    # wxWidgets' own CMake build exports wx:: targets. Adapt them to the
    # wxWidgets:: names used by this project and its tests.
    if(TARGET wx::base AND NOT TARGET wxWidgets::Base)
        add_library(wxWidgets::Base INTERFACE IMPORTED GLOBAL)
        target_link_libraries(wxWidgets::Base INTERFACE wx::base)
    endif()
    if(TARGET wx::core AND NOT TARGET wxWidgets::Core)
        add_library(wxWidgets::Core INTERFACE IMPORTED GLOBAL)
        target_link_libraries(wxWidgets::Core INTERFACE wx::core)
    endif()
    if(TARGET wx::adv AND NOT TARGET wxWidgets::Adv)
        add_library(wxWidgets::Adv INTERFACE IMPORTED GLOBAL)
        target_link_libraries(wxWidgets::Adv INTERFACE wx::adv)
    endif()

    if(NOT TARGET wxWidgets::Base OR NOT TARGET wxWidgets::Core)
        message(FATAL_ERROR
            "wxWidgets was built with FetchContent, but its expected wx::base/wx::core targets were not found.")
    endif()
endif()

# ---------------------------------------------------------------------------
# Catch2 v3 for Automated Unit Testing
# ---------------------------------------------------------------------------
option(BUILD_TESTING "Build automated unit test suite" ON)
if(BUILD_TESTING)
    enable_testing()
    message(STATUS "Configuring Catch2 (v3.x) via FetchContent...")
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.5.3
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    include(Catch)
endif()

# ---------------------------------------------------------------------------
# Project Targets
# ---------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/db/sqlite3.c")
    add_library(sqlite3_lib STATIC src/db/sqlite3.c src/db/sqlite3.h)
    target_include_directories(sqlite3_lib PUBLIC src/db)
    target_compile_definitions(sqlite3_lib PUBLIC SQLITE_THREADSAFE=1)
    if(UNIX AND NOT APPLE)
        target_link_libraries(sqlite3_lib PUBLIC ${CMAKE_DL_LIBS} pthread)
    endif()
    add_library(SQLite::SQLite3 ALIAS sqlite3_lib)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/db/DbService.cpp")
    add_library(msms_db STATIC
        src/db/DbService.cpp
        src/db/DbService.h
        src/util/ErrorReporter.cpp
        src/util/ErrorReporter.h
    )
    target_include_directories(msms_db PUBLIC src src/db src/util)
    target_link_libraries(msms_db PUBLIC sqlite3_lib wxWidgets::Base)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
    file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS "src/*.cpp" "src/*.h")
    add_executable(msms ${APP_SOURCES})
    target_include_directories(msms PRIVATE src)
    target_link_libraries(msms PRIVATE wxWidgets::Core wxWidgets::Base wxWidgets::Adv)
    if(TARGET sqlite3_lib)
        target_link_libraries(msms PRIVATE sqlite3_lib)
    endif()
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()
