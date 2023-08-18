add_library(mpi_compiler_assistance_matching_pass MODULE
        # List your source files here.
        mpi_compiler_assistance_matching_pass.cpp
        mpi_functions.h
        mpi_functions.cpp
        conflict_detection.h
        conflict_detection.cpp
        implementation_specific.h
        implementation_specific.cpp
        debug.h
        analysis_results.h
        analysis_results.cpp
        replacement.h
        replacement.cpp
        nc_settings.h
        json.hpp
        frontend_plugin_data.h
        frontend_plugin_data.cpp
        precalculation.h
        precalculations.cpp
        devirt_analysis.h
        devirt_analysis.cpp
        taintedValue.h
        ptr_info.h
        ptr_info.cpp)

# if one wants to use mpi
#find_package(MPI REQUIRED)
#target_link_libraries(mpi_assertion_checker PRIVATE MPI::MPI_C)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -Og -DDEBUG_MACH_PASS=1")
# switch off one Warning for release build: some variables are only used for assertions
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DDEBUG_MACH_PASS=0 -Wno-unused-but-set-variable")

# Use C++17 to compile our pass (i.e., supply -std=c++11).
target_compile_features(mpi_compiler_assistance_matching_pass PRIVATE cxx_std_17)

ExternalProject_Get_property(ompi INSTALL_DIR)
target_include_directories(mpi_compiler_assistance_matching_pass PUBLIC ${INSTALL_DIR}/include)
target_include_directories(mpi_compiler_assistance_matching_pass PUBLIC ${CMAKE_SOURCE_DIR}/WholeProgramDevirtAnalysisPass)

add_dependencies(mpi_compiler_assistance_matching_pass ompi)


# LLVM is (typically) built with no C++ RTTI. We need to match that;
# otherwise, we'll get linker errors about missing RTTI data.
set_target_properties(mpi_compiler_assistance_matching_pass PROPERTIES
        COMPILE_FLAGS "-fno-rtti -Wall -Wextra -Wno-unused-parameter"
)

