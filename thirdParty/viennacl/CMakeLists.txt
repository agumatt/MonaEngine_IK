find_package(OpenCL REQUIRED)
if(ENABLE_OPENCL)
    add_library(viennacl SHARED libviennacl/src/backend.cpp
                                libviennacl/src/blas1.cpp libviennacl/src/blas1_host.cpp libviennacl/src/blas1_opencl.cpp
                                libviennacl/src/blas2.cpp libviennacl/src/blas2_host.cpp libviennacl/src/blas2_opencl.cpp
                                libviennacl/src/blas3.cpp libviennacl/src/blas3_host.cpp libviennacl/src/blas3_opencl.cpp)
    set_target_properties(viennacl PROPERTIES COMPILE_FLAGS "-DVIENNACL_WITH_OPENCL")
    target_link_libraries(viennacl ${OPENCL_LIBRARIES})
else(ENABLE_OPENCL)
    add_library(viennacl SHARED libviennacl/src/backend.cpp
                                libviennacl/src/blas1.cpp libviennacl/src/blas1_host.cpp
                                libviennacl/src/blas2.cpp libviennacl/src/blas2_host.cpp
                                libviennacl/src/blas3.cpp libviennacl/src/blas3_host.cpp)
endif(ENABLE_OPENCL)
set(vienna_INCLUDE_DIRS 
    "${CMAKE_CURRENT_LIST_DIR}/libviennacl/include/" 
    "${CMAKE_CURRENT_LIST_DIR}/libviennacl/src/"
    "${CMAKE_CURRENT_LIST_DIR}" 
    ${OpenCL_INCLUDE_DIRS})
target_include_directories(viennacl PRIVATE ${vienna_INCLUDE_DIRS})