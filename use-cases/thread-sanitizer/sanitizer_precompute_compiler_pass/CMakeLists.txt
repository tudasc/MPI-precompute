add_library(sanitizer_precompute_pass MODULE
        # List your source files here.
        LoopOptimize.cpp
        sanitizer_precompute_pass.cpp
)

#set(Boost_USE_STATIC_LIBS OFF)
#set(Boost_USE_MULTITHREADED ON)
#set(Boost_USE_STATIC_RUNTIME OFF)
#find_package(Boost 1.66.0 REQUIRED)

#include_directories(${Boost_INCLUDE_DIRS})
#target_link_libraries(sanitizer_precompute_pass ${Boost_LIBRARIES})
target_link_libraries(sanitizer_precompute_pass PRIVATE precomputation_compiler_lib)
target_include_directories(sanitizer_precompute_pass PRIVATE ${CMAKE_SOURCE_DIR}/precomputation_compiler_lib)


#TODO document that teh pass only builds with clang

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fno-omit-frame-pointer -g -Og -DDEBUG_MACH_PASS=1 -gdwarf-4")
# db on my local platform has some issues with gdwarf 5
#TODO the link step does not work correctly, we still need to preload the libasan
#TODO check if compiler is clang
if (CMAKE_BUILD_TYPE MATCHES DEBUG)
    add_link_options(sanitizer_precompute_pass "-fsanitize=address -shared-libasan")
endif (CMAKE_BUILD_TYPE MATCHES DEBUG)
# switch off one Warning for release build: some variables are only used for assertions
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DDEBUG_MACH_PASS=0 -Wno-unused-but-set-variable")

# Use C++17 to compile our pass (i.e., supply -std=c++11).
target_compile_features(sanitizer_precompute_pass PRIVATE cxx_std_17)

target_include_directories(sanitizer_precompute_pass PUBLIC ${CMAKE_SOURCE_DIR}/WholeProgramDevirtAnalysisPass)

# LLVM is (typically) built with no C++ RTTI. We need to match that;
# otherwise, we'll get linker errors about missing RTTI data.
set_target_properties(sanitizer_precompute_pass PROPERTIES
        COMPILE_FLAGS "-fno-rtti -Wall -Wextra -Wno-unused-parameter"
)

