# This file is part of the rc_dynamics_api package.
#
# Copyright (c) 2017 Roboception GmbH
# All rights reserved
#
# Author: Christian Emmerich
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
# this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimer in the documentation
# and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its contributors
# may be used to endorse or promote products derived from this software without
# specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.


cmake_minimum_required (VERSION 2.8.12)

# Don't set PROJECT_VERSION to empty string when no VERSION is given to project() command.
if(POLICY CMP0048)
  cmake_policy(SET CMP0048 OLD)
endif()

project(rc_dynamics_api)
include(cmake/project_version.cmake)

enable_testing()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Find protocol buffers

SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/")
FIND_PACKAGE(ProtocolBuffers REQUIRED)
IF(NOT PROTOBUF_LIBRARY)
    MESSAGE(FATAL_ERROR "protobuf library not found")
ENDIF()
IF(NOT PROTOBUF_INCLUDE_DIR)
    MESSAGE(FATAL_ERROR "protobuf include dir not found")
ENDIF()
IF(NOT PROTOBUF_PROTOC_EXECUTABLE)
    MESSAGE(FATAL_ERROR "protoc executable not found")
ENDIF()

INCLUDE_DIRECTORIES(${PROTOBUF_INCLUDE_DIR})

# - Configuration for code optimization -

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RELEASE CACHE STRING "Build type: DEBUG or RELEASE" FORCE)
endif ()

include(cmake/optimization_flags.cmake)

# - Standard definitions -

add_definitions(-Wall)
add_definitions(-std=c++11)

if (CMAKE_COMPILER_IS_GNUCC)
  add_definitions(-Wno-unknown-pragmas)
endif ()

if (MSVC)
  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
  
  # Switching off compiler warnings in sub-modules and generated code

  add_definitions("/wd4018")
  add_definitions("/wd4061")
  add_definitions("/wd4090")
  add_definitions("/wd4125")
  add_definitions("/wd4146")
  add_definitions("/wd4244")
  add_definitions("/wd4245")
  add_definitions("/wd4251")
  add_definitions("/wd4255")
  add_definitions("/wd4265")
  add_definitions("/wd4267")
  add_definitions("/wd4305")
  add_definitions("/wd4310")
  add_definitions("/wd4355")
  add_definitions("/wd4365")
  add_definitions("/wd4514")
  add_definitions("/wd4548")
  add_definitions("/wd4571")
  add_definitions("/wd4574")
  add_definitions("/wd4625")
  add_definitions("/wd4626")
  add_definitions("/wd4647")
  add_definitions("/wd4668")
  add_definitions("/wd4701")
  add_definitions("/wd4706")
  add_definitions("/wd4710")
  add_definitions("/wd4711")
  add_definitions("/wd4774")
  add_definitions("/wd4800")
  add_definitions("/wd4820")
  add_definitions("/wd4946")
  add_definitions("/wd4996")
  add_definitions("/wd5026")
  add_definitions("/wd5027")
endif ()

# - Build individual parts -

option(BUILD_DOC "Add target for building doxygen docs" ON)
option(BUILD_EXAMPLES "Build test programs" ON)
option(BUILD_TOOLS "Build commandline tools" ON)
option(BUILD_SHARED_LIBS "Build shared libs" ON)

add_subdirectory(rc_dynamics_api)
if (BUILD_TOOLS)
  add_subdirectory(tools)
endif ()
if (BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

# export project targets

# Declare (shared) libraries that are created by your project. For all
# libraries that appear here, the following line should exist in a cmake
# file in a sub directory:
# install(TARGETS rcXXX EXPORT PROJECTTargets COMPONENT bin DESTINATION lib)
if (BUILD_SHARED_LIBS)
  set(PROJECT_LIBRARIES rc_dynamics_api)
endif ()

# Declare static libraries that are created by your project. For all
# libraries that appear here, the following line should exist in a cmake
# file in a sub directory:
# install(TARGETS rcXXX_static EXPORT PROJECTTargets COMPONENT dev DESTINATION lib)
#set(PROJECT_STATIC_LIBRARIES rc_dynamics_api_static)

include(cmake/configure_link_libs.cmake)

# add a target to generate API documentation with Doxygen
if (BUILD_DOC)
  add_subdirectory(doc)
endif()

# - Define information for packaging -

# optionally set description and maintainer (if not set will use defaults from package_debian.cmake)
#set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "Roboception package description")

# optionally set package maintainer (otherwise defaults to Roboception <info@roboception.de>)
#set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Your Name <your.name@roboception.de>")

# optionally specify dependencies of the debian package here (comma separated!)
#set(CPACK_DEBIAN_PACKAGE_DEPENDS "genicam (>= 1.0.0)")

include(cmake/package_debian.cmake)
