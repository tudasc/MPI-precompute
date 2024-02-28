# CMake entry point
cmake_minimum_required(VERSION 3.0.2)

project(murb)

# Command line options
option(ENABLE_MURB "Enable to compile the MUrB executable" ON)
option(ENABLE_VISU "Enable the OpenGL visualization" ON)
option(ENABLE_MURB_READER "Enable to compile the MUrB reader executable" ON)
option(ENABLE_MURB_MPI "Enable to compile the MUrB MPI executable" OFF)

# Set the path to the CMake modules
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

# Specify bin path
set(EXECUTABLE_OUTPUT_PATH bin/${CMAKE_BUILD_TYPE})

# Enable C++11
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
    set(CMAKE_CXX_STANDARD 14)
else ()
    set(CMAKE_CXX_STANDARD 11)
endif ()

if (ENABLE_VISU)
    set(OpenGL_GL_PREFERENCE LEGACY)
    find_package(OpenGL)
    find_package(GLEW)
    find_package(GLM)
    find_package(GLFW)
    find_package(X11)

    if ((OPENGL_FOUND) AND (GLEW_FOUND) AND (GLM_FOUND) AND (GLFW_FOUND))
        set(LINK_VISU 1)
        set(COMPILE_READER 1)
    else ()
        message(STATUS "The visualization has been disabled.")
        set(COMPILE_READER 0)
        set(LINK_VISU 0)
    endif ()
endif ()

# Génération de la liste des fichiers sources
file(GLOB_RECURSE source_common src/common/*)

# Object
add_library(common-lib OBJECT ${source_common})

if (ENABLE_MURB)
    # Génération de la liste des fichiers sources
    file(GLOB_RECURSE source_murb_files src/murb/*)

    # Déclaration de l'exécutable
    add_executable(murb-bin $<TARGET_OBJECTS:common-lib> ${source_murb_files})
    set_target_properties(murb-bin PROPERTIES OUTPUT_NAME murb)
endif ()

if ((ENABLE_MURB_READER) AND (COMPILE_READER))
    # Génération de la liste des fichiers sources
    file(GLOB_RECURSE source_reader_files src/reader/*)

    # Déclaration de l'exécutable
    add_executable(reader-bin $<TARGET_OBJECTS:common-lib> ${source_reader_files})
    set_target_properties(reader-bin PROPERTIES OUTPUT_NAME reader)
endif ()

macro(murb_link_libraries lib)
    if (ENABLE_MURB)
        target_link_libraries(murb-bin ${lib})
    endif (ENABLE_MURB)
    if (ENABLE_MURB_READER)
        target_link_libraries(reader-bin ${lib})
    endif (ENABLE_MURB_READER)
endmacro()

# MIPP wrapper includes
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/lib/MIPP/src/mipp.h")
    add_definitions(-DMIPP_ENABLE_BACKTRACE)
    include_directories("${CMAKE_CURRENT_SOURCE_DIR}/lib/MIPP/src/")
    message(STATUS "MIPP found")
else ()
    message(FATAL_ERROR "MIPP can't be found, try to init the submodules with the following cmd:\n"
            "$ git submodule update --init -- ../lib/MIPP/")
endif ()

if (LINK_VISU)
    message(STATUS "Link with OpenGL libraries")
    #   find_package (OpenGL REQUIRED)
    if (OPENGL_FOUND)
        # include_directories (${OPENGL_INCLUDE_DIR})
        # murb_link_libraries ("${OPENGL_LIBRARIES}")
        murb_link_libraries(OpenGL::OpenGL)
        murb_link_libraries(OpenGL::GLX)
    endif (OPENGL_FOUND)

    #   find_package (GLEW REQUIRED)
    if (GLEW_FOUND)
        include_directories(${GLEW_INCLUDE_DIRS})
        murb_link_libraries("${GLEW_LIBRARIES}")
    endif (GLEW_FOUND)

    #   find_package (GLM REQUIRED)
    if (GLM_FOUND)
        include_directories(${GLM_INCLUDE_DIRS})
    endif (GLM_FOUND)

    #   find_package (GLFW REQUIRED)
    if (GLFW_FOUND)
        include_directories(${GLFW_INCLUDE_DIR})
        murb_link_libraries("${GLFW_LIBRARIES}")
    endif (GLFW_FOUND)

    # Enable visualization
    add_definitions(-DVISU)
endif ()

if ((ENABLE_MURB_MPI) AND (ENABLE_MURB))
    # Enable MPI
    add_definitions(-DUSE_MPI)

    find_package(MPI REQUIRED)
    if (MPI_CXX_FOUND)
        message(STATUS "MPI found")
        include_directories(${MPI_CXX_INCLUDE_PATH})
        murb_link_libraries("${MPI_CXX_LIBRARIES}")

        if (MPI_CXX_COMPILE_FLAGS)
            set_target_properties(murb-bin PROPERTIES COMPILE_FLAGS "${MPI_CXX_COMPILE_FLAGS}")
        endif (MPI_CXX_COMPILE_FLAGS)

        if (MPI_CXX_LINK_FLAGS)
            set_target_properties(murb-bin PROPERTIES LINK_FLAGS "${MPI_CXX_LINK_FLAGS}")
        endif (MPI_CXX_LINK_FLAGS)

    endif (MPI_CXX_FOUND)
endif ()
