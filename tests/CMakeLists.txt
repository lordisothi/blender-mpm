# Tests

if(BUILD_TESTS)
    enable_testing()
    
    # Add test executable
    add_executable(mpm_tests
        test_basic.cpp
    )
    
    target_link_libraries(mpm_tests
        mpm_core
        mpm_cuda
    )
    
    add_test(NAME basic_test COMMAND mpm_tests)
endif()
