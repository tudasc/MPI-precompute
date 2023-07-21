
ExternalProject_Get_property(ompi INSTALL_DIR)

set(CMAKE_C_COMPILER ${INSTALL_DIR}/bin/mpicc)

#set_source_files_properties(one-sided-persistent.c PROPERTIES GENERATED TRUE)
    #add example executable for testing the implementation
add_executable(implementation_test_example
        example.c
        ../one-sided-persistent.c )

target_include_directories(implementation_test_example PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/.. ${INSTALL_DIR}/include)

target_link_libraries(implementation_test_example open-pal ucp m)
add_dependencies(implementation_test_example ompi)


#endif(CMAKE_BUILD_TYPE MATCHES Debug)

