cmake_minimum_required(VERSION 3.21)

project(MSMSManager
    VERSION 1.0.0
    DESCRIPTION "MSMS Membership and Administration"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Sql Test)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Core library with database, schema, and reusable UI components
add_library(msms_core STATIC
    src/database/Schema.h
    src/database/Schema.cpp
    src/database/DatabaseManager.h
    src/database/DatabaseManager.cpp
    src/ui/Theme.h
    src/ui/Theme.cpp
    src/ui/DatabaseSelectionDialog.h
    src/ui/DatabaseSelectionDialog.cpp
    src/ui/AllMembersView.h
    src/ui/AllMembersView.cpp
    src/ui/PlaceholderView.h
    src/ui/PlaceholderView.cpp
    src/ui/QueryConsoleView.h
    src/ui/QueryConsoleView.cpp
    src/ui/RawTableView.h
    src/ui/RawTableView.cpp
)

target_link_libraries(msms_core
    PUBLIC
        Qt6::Widgets
        Qt6::Sql
)

# Main executable
qt_add_executable(MSMSManager
    main.cpp
    mainwindow.cpp
    mainwindow.h
)

target_link_libraries(MSMSManager
    PRIVATE
        msms_core
        Qt6::Widgets
        Qt6::Sql
)

# Automated testing
enable_testing()

qt_add_executable(MSMSTests
    tests/test_database.cpp
)

target_link_libraries(MSMSTests
    PRIVATE
        msms_core
        Qt6::Test
        Qt6::Widgets
        Qt6::Sql
)

add_test(NAME MSMSTests COMMAND MSMSTests)

include(GNUInstallDirs)

install(TARGETS MSMSManager
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
