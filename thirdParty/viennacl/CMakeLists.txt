# Project setup
###############

cmake_minimum_required(VERSION 2.9 FATAL_ERROR)
if(COMMAND cmake_policy)
  cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)


project(ViennaCL)

set(VERSION_MAJOR 1)
set(VERSION_MINOR 7)
set(VERSION_PATCH 1)
set(VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

SET(CMAKE_VERBOSE_MAKEFILE ON)   #uncomment this line to increase CMake verbosity

#SET(BOOSTPATH "C:/Program\ Files\ (x86)/boost/boost_1_42")    #Boost path for other systems - customize as needed

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# Add visibility of headers
# Necessary for Qt-Creator usage.
################


# Common options
################

set(VIENNACL_SRC_DIST TRUE)
include(ViennaCLCommon)

# Set release build:
IF(DEFINED CMAKE_BUILD_TYPE)
 SET (CMAKE_BUILD_TYPE "${CMAKE_BUILD_TYPE}")
ELSE()
 SET (CMAKE_BUILD_TYPE Release)
ENDIF()

# Compile options
#################

include_directories(BEFORE "${PROJECT_SOURCE_DIR}")


# Subdirectories
################

if(BUILD_EXAMPLES)
   add_subdirectory(examples)
endif()
add_subdirectory(libviennacl)

# Install
#########


include_directories(${PROJECT_SOURCE_DIR}/libviennacl/include/)
include_directories(${PROJECT_SOURCE_DIR}/libviennacl/)

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

# Special linkage for OpenMP under MinGW:
if(ENABLE_OPENMP AND MINGW)
   target_link_libraries(viennacl gomp)
endif(ENABLE_OPENMP AND MINGW)

# Dist target
#############