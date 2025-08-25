include(FetchContent)

# use DataRaceBench as test
FetchContent_Declare(
        drb
        GIT_REPOSITORY https://github.com/LLNL/dataracebench.git
        GIT_TAG v1.4.1
        GIT_PROGRESS TRUE
)
# ensure it is downloaded

message("Download DataRaceBench for testing")
FetchContent_MakeAvailable(drb)

# Find all C files in micro_benches and create tests
file(GLOB BENCHMARK_SOURCES "${drb_SOURCE_DIR}/micro-benchmarks/*.c")
list(LENGTH BENCHMARK_SOURCES LEN_BENCHMARK_SOURCES)

message("Import ${LEN_BENCHMARK_SOURCES} Testcases from DataRaceBench")

function(add_compare_test SRC_FILE)
    get_filename_component(TEST_NAME ${SRC_FILE} NAME_WE) # Extract filename without extension

    add_test(NAME ${TEST_NAME}
            COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/compare.sh ${USECASE_SANITIZER_BINARY_DIR} ${SRC_FILE}
    )
endfunction()


# Define a test for each code in DRB
foreach (DRB_BENCHMARK_SRC_FILE ${BENCHMARK_SOURCES})
    add_compare_test(${DRB_BENCHMARK_SRC_FILE})
endforeach ()

#cmakes cpp try compile
add_compare_test(${CMAKE_CURRENT_SOURCE_DIR}/try-compile.cpp)

# pthreads
file(GLOB PTHREAD_TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/pthreads/pthread_*.c")
foreach (PTHREAD_TEST ${PTHREAD_TEST_SOURCES})
    add_compare_test(${PTHREAD_TEST})
endforeach ()


