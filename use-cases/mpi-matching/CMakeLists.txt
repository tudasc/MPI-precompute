
set(USECASE_MPI_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(openmpi-patch)
add_subdirectory(mpi_compiler_assistance_matching_pass)
add_subdirectory(tests)

#TODO complete refactoring for this part
ExternalProject_Get_property(ompi INSTALL_DIR)
set(OMPI_INSTALL_DIR ${INSTALL_DIR})

add_custom_target(setup_env.sh ALL
        COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/setup_env.sh.in ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export PATH=${OMPI_INSTALL_DIR}/bin:\\$$PATH" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export MPICC=${OMPI_INSTALL_DIR}/bin/mpicc" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export MPICXX=${OMPI_INSTALL_DIR}/bin/mpicxx" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export CPATH=${OMPI_INSTALL_DIR}/include:\\$$CPATH" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export LIBRARY_PATH=${OMPI_INSTALL_DIR}/lib:${PRECOMPUTE_LIB_PATH}:\\$$LIBRARY_PATH" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export LD_LIBRARY_PATH=${OMPI_INSTALL_DIR}/lib:${PRECOMPUTE_LIB_PATH}:\\$$LD_LIBRARY_PATH" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export MPI_COMPILER_ASSISTANCE_PASS=$<TARGET_FILE:mpi_compiler_assistance_matching_pass>" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export COMPILER_PASS=$<TARGET_FILE:mpi_compiler_assistance_matching_pass>" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export WHOLE_PROGRAM_DEVIRT_ANALYSIS_PASS=${CMAKE_BINARY_DIR}/WholeProgramDevirtAnalysisPass/libwholeProgramDevirtAnalysisPass.so" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export OMPI_CC=${CMAKE_CURRENT_BINARY_DIR}/clang_wrap_cc" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export OMPI_CXX=${CMAKE_CURRENT_BINARY_DIR}/clang_wrap_cxx" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export CLANG_WRAP_CC=${CMAKE_CURRENT_BINARY_DIR}/clang_wrap_cc" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export CLANG_WRAP_CXX=${CMAKE_CURRENT_BINARY_DIR}/clang_wrap_cxx" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh

        DEPENDS setup_env.sh.in)

add_custom_target(run.sh ALL
        COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/run.sh ${CMAKE_CURRENT_BINARY_DIR}/run.sh
        DEPENDS run.sh)

add_custom_target(clang_wrap_cc ALL
        COMMAND cp ${CMAKE_SOURCE_DIR}/clang_wrapper.sh ${CMAKE_CURRENT_BINARY_DIR}/clang_wrap_cc && sed -i '3s/.*/compiler=clang/' ${CMAKE_CURRENT_BINARY_DIR}/clang_wrap_cc
        DEPENDS ${CMAKE_SOURCE_DIR}/clang_wrapper.sh)

add_custom_target(clang_wrap_cxx ALL
        COMMAND cp ${CMAKE_SOURCE_DIR}/clang_wrapper.sh ${CMAKE_CURRENT_BINARY_DIR}/clang_wrap_cxx && sed -i '3s/.*/compiler=clang++/' ${CMAKE_CURRENT_BINARY_DIR}/clang_wrap_cxx
        DEPENDS ${CMAKE_SOURCE_DIR}/clang_wrapper.sh)
