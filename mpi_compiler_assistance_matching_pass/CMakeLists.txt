add_library(mpi_compiler_assistance_matching_pass MODULE
        # List your source files here.
        mpi_compiler_assistance_matching_pass.cpp
        mpi_functions.h
        mpi_functions.cpp
        implementation_specific.h
        implementation_specific.cpp
        debug.h
        debug.cpp
        analysis_results.h
        analysis_results.cpp
        replacement.h
        replacement.cpp
        nc_settings.h
        precalculation.h
        precalculations.cpp
        devirt_analysis.h
        devirt_analysis.cpp
        taintedValue.h
        ptr_info.h
        ptr_info.cpp
        CompilerPassConstants.h
        precompute_funcs.cpp
        precompute_funcs.h
        Precompute_insertion.cpp
        Precompute_insertion.h
        VtableManager.cpp
        VtableManager.h
)

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost 1.66.0 REQUIRED)

include_directories(${Boost_INCLUDE_DIRS})
target_link_libraries(mpi_compiler_assistance_matching_pass ${Boost_LIBRARIES})

# if one wants to use mpi
#find_package(MPI REQUIRED)
#target_link_libraries(mpi_assertion_checker PRIVATE MPI::MPI_C)


#TODO document that teh pass only builds with clang

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fno-omit-frame-pointer -g -Og -DDEBUG_MACH_PASS=1 -gdwarf-4")
# db on my local platform has some issues with gdwarf 5
#TODO the link step does not work correctly, we still need to preload the libasan
#TODO check if compiler is clang
if (CMAKE_BUILD_TYPE MATCHES DEBUG)
    add_link_options(mpi_compiler_assistance_matching_pass  "-fsanitize=address -shared-libasan")
endif (CMAKE_BUILD_TYPE MATCHES DEBUG)
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

