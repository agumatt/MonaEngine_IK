include_directories(${PROJECT_SOURCE_DIR}/libviennacl/include/)
find_package(OpenCL REQUIRED)
if(ENABLE_OPENCL)
    add_library(viennacl SHARED src/backend.cpp
                                src/blas1.cpp src/blas1_host.cpp src/blas1_opencl.cpp
                                src/blas2.cpp src/blas2_host.cpp src/blas2_opencl.cpp
                                src/blas3.cpp src/blas3_host.cpp src/blas3_opencl.cpp)
    set_target_properties(viennacl PROPERTIES COMPILE_FLAGS "-DVIENNACL_WITH_OPENCL")
    target_link_libraries(viennacl ${OPENCL_LIBRARIES})
else(ENABLE_OPENCL)
    add_library(viennacl SHARED src/backend.cpp
                                src/blas1.cpp src/blas1_host.cpp
                                src/blas2.cpp src/blas2_host.cpp
                                src/blas3.cpp src/blas3_host.cpp)
endif(ENABLE_OPENCL)
set(vienna_INCLUDE_DIRS 
    "${PROJECT_SOURCE_DIR}/libviennacl/include/" 
    "${PROJECT_SOURCE_DIR}/libviennacl/src/"
    "${PROJECT_SOURCE_DIR}" 
    ${OpenCL_INCLUDE_DIRS})
target_include_directories(viennacl PRIVATE ${vienna_INCLUDE_DIRS})


# Special linkage for OpenMP under MinGW:
if(ENABLE_OPENMP AND MINGW)
   target_link_libraries(viennacl gomp)
endif(ENABLE_OPENMP AND MINGW)