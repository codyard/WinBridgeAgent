# Unit tests (open-source edition)
#
# Current tests are standalone executables using assert() and their own main().
# Build each test file as a separate binary so we don't get multiple-definition
# errors when linking.

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "unit/test_*.cpp")

foreach(test_file ${TEST_SOURCES})
    get_filename_component(test_name ${test_file} NAME_WE)

    add_executable(${test_name} ${test_file})
    target_link_libraries(${test_name} PRIVATE clawdesk_lib)

    if(WIN32)
        target_link_libraries(${test_name} PRIVATE
            ws2_32 gdi32 gdiplus user32 shell32 comdlg32 advapi32 ole32 uuid
            shlwapi psapi powrprof comctl32 crypt32 dbghelp winhttp
        )
        if(CLAWDESK_ENABLE_OPENSSL)
            target_link_libraries(${test_name} PRIVATE ssl crypto)
        endif()
    endif()

    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

