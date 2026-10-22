# Unit tests for MSMS Manager
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_db_service.cpp")
    add_executable(test_db_service
        test_db_service.cpp
    )
    target_link_libraries(test_db_service PRIVATE
        Catch2::Catch2WithMain
    )
    if(TARGET sqlite3_lib)
        target_link_libraries(test_db_service PRIVATE sqlite3_lib)
    endif()
    if(TARGET wxWidgets::Base)
        target_link_libraries(test_db_service PRIVATE wxWidgets::Base)
    endif()
    catch_discover_tests(test_db_service)
endif()
