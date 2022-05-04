# - Find the OpenCL headers and library
#
# Defines the following if found:
#  OPENCL_FOUND        : TRUE if found, FALSE otherwise
#  OPENCL_INCLUDE_DIRS : Include directories for OpenCL
#  OPENCL_LIBRARIES    : The libraries to link against
#
# The user can set the OPENCLROOT environment variable to help finding OpenCL
# if it is installed in a non-standard place.

find_path(OPENCL_INCLUDE_DIR
  NAMES
    CL/cl.h OpenCL/cl.h
  PATHS
    ENV "PROGRAMFILES(X86)"
    ENV AMDAPPSDKROOT
    ENV INTELOCLSDKROOT
    ENV NVSDKCOMPUTE_ROOT
    ENV CUDA_PATH
    ENV ATISTREAMSDKROOT
    ENV OCL_ROOT
    "${PROJECT_SOURCE_DIR}"
  PATH_SUFFIXES
    include
    OpenCL/common/inc
    "AMD APP/include")

if(WIN32)
  if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    find_library(OPENCL_LIBRARY
      NAMES OpenCL
      PATHS
        ENV "PROGRAMFILES(X86)"
        ENV AMDAPPSDKROOT
        ENV INTELOCLSDKROOT
        ENV CUDA_PATH
        ENV NVSDKCOMPUTE_ROOT
        ENV ATISTREAMSDKROOT
        ENV OCL_ROOT
      PATH_SUFFIXES
        "AMD APP/lib/x86"
        lib/x86
        lib/Win32
        OpenCL/common/lib/Win32)
  elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    find_library(OPENCL_LIBRARY
      NAMES OpenCL
      PATHS
        ENV "PROGRAMFILES(X86)"
        ENV AMDAPPSDKROOT
        ENV INTELOCLSDKROOT
        ENV CUDA_PATH
        ENV NVSDKCOMPUTE_ROOT
        ENV ATISTREAMSDKROOT
        ENV OCL_ROOT
      PATH_SUFFIXES
        "AMD APP/lib/x86_64"
        lib/x86_64
        lib/x64
        OpenCL/common/lib/x64)
  endif()
else()
  if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    find_library(OPENCL_LIBRARY
      NAMES OpenCL
      PATHS
        ENV AMDAPPSDKROOT
        ENV CUDA_PATH
      PATH_SUFFIXES
        lib/x86
        lib)
  elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    find_library(OPENCL_LIBRARY
      NAMES OpenCL
      PATHS
        ENV AMDAPPSDKROOT
        ENV CUDA_PATH
      PATH_SUFFIXES
        lib/x86_64
        lib/x64
        lib
        lib64)
  endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  OPENCL
  DEFAULT_MSG
  OPENCL_LIBRARY OPENCL_INCLUDE_DIR
  )

if(OPENCL_FOUND)
  set(OPENCL_INCLUDE_DIRS "${OPENCL_INCLUDE_DIR}")
  set(OPENCL_LIBRARIES "${OPENCL_LIBRARY}")
else(OPENCL_FOUND)
  set(OPENCL_INCLUDE_DIRS)
  set(OPENCL_LIBRARIES)
endif(OPENCL_FOUND)

mark_as_advanced(
  OPENCL_INCLUDE_DIR
  OPENCL_LIBRARY
  )
