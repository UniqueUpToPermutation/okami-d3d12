# CTest configuration for Engine project

# Set the site name for CDash (if using dashboard)
set(CTEST_SITE "Local")

# Set the build name
set(CTEST_BUILD_NAME "Engine-Tests")

# Test timeout (in seconds)
set(CTEST_TEST_TIMEOUT 300)

# Output options
set(CTEST_OUTPUT_ON_FAILURE TRUE)

# Memory checking (if valgrind is available)
# set(CTEST_MEMORYCHECK_COMMAND "/usr/bin/valgrind")

# Coverage options (if gcov is available)  
# set(CTEST_COVERAGE_COMMAND "/usr/bin/gcov")

# Parallel testing
include(ProcessorCount)
ProcessorCount(N)
if(NOT N EQUAL 0)
    set(CTEST_BUILD_FLAGS -j${N})
    set(CTEST_TEST_ARGS ${CTEST_TEST_ARGS} PARALLEL_LEVEL ${N})
endif()