
include(ExternalProject)

cmake_policy(SET CMP0114 NEW)

#TODO
#find_package(HWLOC REQUIRED)
#find_package(UCX REQUIRED)  or anways use the openmpi internals ucx??
# and pass the paths to ompi configure

#TODO expose a cmake variable to specify the mpi build options
# for the cluster openmpi build
#set(OMPI_CONFIGURE_FLAGS
#        --enable-heterogeneous --enable-static --with-hwloc=${HWLOC_ROOT}
#        --with-slurm=/opt/slurm/current --with-pmi=/opt/slurm/current
#        --with-ucx --enable-mca-no-build=btl-uct --without-verbs --enable-mpi-fortran=no
#        LIBS=-lprecompute LDFLAGS=-L${CMAKE_BINARY_DIR}/precompute_lib
#)

# for my local openmpi build 
set(OMPI_CONFIGURE_FLAGS
        --enable-heterogeneous --enable-static
        --with-ucx --enable-mca-no-build=btl-uct --without-verbs --enable-mpi-fortran=no
        LIBS=-lprecompute LDFLAGS=-L${CMAKE_BINARY_DIR}/precompute_lib CFLAGS=-gdwarf-4 CXXFLAGS=-gdwarf-4
)


if (CMAKE_BUILD_TYPE MATCHES DEBUG)
    set(OMPI_CONFIGURE_FLAGS ${OMPI_CONFIGURE_FLAGS} --enable-debug)
endif (CMAKE_BUILD_TYPE MATCHES DEBUG)

#TODO assert that generator is makefiles
#or also support ninja

ExternalProject_Add(
        ompi
        URL https://download.open-mpi.org/release/open-mpi/v4.1/openmpi-4.1.1.tar.gz
        URL_HASH MD5=8239df775478285fb14edf37d02f5bfa
        PATCH_COMMAND patch <SOURCE_DIR>/ompi/mca/osc/ucx/Makefile.in ${CMAKE_CURRENT_SOURCE_DIR}/patches/ompi_makefile.patch && patch <SOURCE_DIR>/ompi/mca/osc/ucx/osc_ucx_component.c ${CMAKE_CURRENT_SOURCE_DIR}/patches/ompi_osc_ucx_component.patch
        CONFIGURE_COMMAND
        ${CMAKE_COMMAND} -E env LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${CMAKE_BINARY_DIR}/precompute_lib
        <SOURCE_DIR>/configure --prefix <INSTALL_DIR> CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER} CFLAGS=${CMAKE_C_FLAGS} CXXFLAGS=${CMAKE_CXX_FLAGS} ${OMPI_CONFIGURE_FLAGS}
        BUILD_COMMAND $(MAKE)
        DOWNLOAD_EXTRACT_TIMESTAMP true
        DEPENDS precompute
)

ExternalProject_Add_Step(ompi patch_header
        COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/patches/patch_ompi_header.sh ${CMAKE_CURRENT_SOURCE_DIR}/interface.h <SOURCE_DIR>/ompi/include/mpi.h.in
        DEPENDEES patch
        DEPENDERS configure
       )

# include the file so that the compiler does not complain that it is missing
ExternalProject_Add_Step(ompi include_empty_implementation
        COMMAND echo "" > <SOURCE_DIR>/ompi/mca/osc/ucx/one-sided-persistent.c
        DEPENDEES configure
        DEPENDERS build
        )

# we need to re-build to include the implementation
# the implementation can only be built when all mpi internal headers have been build
ExternalProject_Add_Step(ompi build_with_implementation
        COMMAND $(MAKE)
        WORKING_DIRECTORY <BINARY_DIR>
        DEPENDEES include_implementation
        DEPENDERS install
        )


ExternalProject_Add_Step(ompi include_implementation
        COMMAND ${CMAKE_COMMAND} -E copy_if_different one-sided-persistent.c <SOURCE_DIR>/ompi/mca/osc/ucx/one-sided-persistent.c
        DEPENDEES build
        DEPENDERS build_with_implementation
        DEPENDS one-sided-persistent.c
        )

ExternalProject_Add_StepTargets(ompi build)

ExternalProject_Get_property(ompi SOURCE_DIR INSTALL_DIR BINARY_DIR)

# source dir and binary dir, as some headers are in the source dir and others are created at build time
set(persistent_implementation-include -I${BINARY_DIR} -I${BINARY_DIR}/opal/include -I${BINARY_DIR}/ompi/include/ -I${BINARY_DIR}/orte/include
        -I${SOURCE_DIR} -I${SOURCE_DIR}/opal/include -I${SOURCE_DIR}/ompi/include/ -I${SOURCE_DIR}/orte/include
        -I${CMAKE_SOURCE_DIR}/precompute_lib
        -I${CMAKE_SOURCE_DIR}/mpi_compiler_assistance_matching_pass
)

set(PRECOMPUTE_H_FILE ${CMAKE_SOURCE_DIR}/precompute_lib/precompute.h)
set(COMPILER_PASS_CONSTANTS_H_FILE ${CMAKE_SOURCE_DIR}/mpi_compiler_assistance_matching_pass/CompilerPassConstants.h)
# depends on the header file not the implementation of the library

set(PERSISTENT_IMPLEMENTATION_FILES debug.c debug.h finalization.c finalization.h globals.c globals.h handshake.c handshake.h initialization.c initialization.h interface.c interface.h mpi-internals.h pack.c pack.h request_type.h settings.h start.c start.h test.c test.h wait.c wait.h)

add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/one-sided-persistent.c
        COMMAND ${CMAKE_C_COMPILER} -E ${CFLAGS} ${persistent_implementation-include} -DLINKAGE_TYPE=static ${CMAKE_CURRENT_SOURCE_DIR}/one-sided-persistent.h -o ${CMAKE_CURRENT_BINARY_DIR}/one-sided-persistent.c
        COMMENT "build one sided persistent implementation"
        DEPENDS ompi-build precompute ${PERSISTENT_IMPLEMENTATION_FILES} ${PRECOMPUTE_H_FILE} ${COMPILER_PASS_CONSTANTS_H_FILE}
        )
