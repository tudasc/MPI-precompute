add_library(wholeProgramDevirtAnalysisPass MODULE
        # List your source files here.
        WholeProgramDevirtAnalysis.h
        WholeProgramDevirtAnalysis.cpp)


set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -Og -DDEBUG_MACH_PASS=1")
# switch off one Warning for release build: some variables are only used for assertions
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DDEBUG_MACH_PASS=0 -Wno-unused-but-set-variable")

# Use C++17 to compile our pass (i.e., supply -std=c++17).
target_compile_features(wholeProgramDevirtAnalysisPass PRIVATE cxx_std_17)

# LLVM is (typically) built with no C++ RTTI. We need to match that;
# otherwise, we'll get linker errors about missing RTTI data.
set_target_properties(wholeProgramDevirtAnalysisPass PROPERTIES
        COMPILE_FLAGS "-fno-rtti -Wall -Wextra -Wno-unused-parameter"
)
