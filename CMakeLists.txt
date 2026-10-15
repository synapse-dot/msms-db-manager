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

find_package(Qt6 REQUIRED COMPONENTS Widgets Sql)

qt_add_executable(MSMSManager
    main.cpp
    mainwindow.cpp
    mainwindow.h
)

target_link_libraries(MSMSManager
    PRIVATE
        Qt6::Widgets
        Qt6::Sql
)

include(GNUInstallDirs)

install(TARGETS MSMSManager
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
