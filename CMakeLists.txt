set(CMAKE_LEGACY_CYGWIN_WIN32 OFF)
cmake_minimum_required(VERSION 3.15)
project(MonaEngine C CXX)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
find_package(OpenGL REQUIRED)
find_package(Python REQUIRED COMPONENTS Development)
set(THIRD_PARTY_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/glad/include"
						"${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/glfw-3.3.2/include"
						"${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/spdlog-1.9.2/include"
						"${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/glm-0.9.9.8"
						"${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/imgui-1.78"
						"${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/bullet3-2.88/src"
						"${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/openal-soft/include"
						"${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/dr_wav"
						"${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/assimp-5.0.1/include"
						"${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/stb"
						"${Python_INCLUDE_DIRS}"
						"${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/viennacl")
set(MONA_INCLUDE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/source")
if(NOT ${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
	set(THIRD_PARTY_INCLUDE_DIRECTORIES ${THIRD_PARTY_INCLUDE_DIRECTORIES} PARENT_SCOPE)
	set(MONA_INCLUDE_DIRECTORY ${MONA_INCLUDE_DIRECTORY} PARENT_SCOPE)
endif()
list (APPEND BULLET_LIBRARIES BulletDynamics)
list (APPEND BULLET_LIBRARIES BulletCollision)
list (APPEND BULLET_LIBRARIES LinearMath)
if(MSVC)
	set(THIRD_PARTY_LIBRARIES glad glfw ${OPENGL_LIBRARIES} ImGui ${BULLET_LIBRARIES} OpenAL dr_wav assimp stb Python::Python viennacl)
else()
	set(THIRD_PARTY_LIBRARIES glad glfw ${OPENGL_LIBRARIES} ImGui ${BULLET_LIBRARIES} stdc++fs OpenAL dr_wav assimp stb Python::Python viennacl)
endif(MSVC)
configure_file(CMakeConfigFiles/RootDirectory.hpp.in "${CMAKE_CURRENT_SOURCE_DIR}/source/Core/RootDirectory.hpp")
add_subdirectory(thirdParty)
add_subdirectory(source)
add_subdirectory(tests)
add_subdirectory(examples)
