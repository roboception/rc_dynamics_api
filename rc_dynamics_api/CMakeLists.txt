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

project(rc_dynamics_api CXX)

## Adding cpr and json library.
##############################
add_subdirectory(opt)
include_directories(${CPR_INCLUDE_DIRS} ${JSON_INCLUDE_DIRS})


## Compiling and building protobuf messages into a library
##########################################################

# let shared objects include static libs
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Find protocol buffers
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
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

# set correct include dirs
INCLUDE_DIRECTORIES(BEFORE ${CMAKE_CURRENT_BINARY_DIR})

# List of absolute file names to each proto file
SET(PROTOS)

# find all proto files relative to this source dir
FILE(GLOB_RECURSE PROTOS_REL RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/roboception/msgs/[^._]*.proto")

# find all proto files, store them in a list for later processing, and copy them to the binary dir
FOREACH(FILE ${PROTOS_REL})
    SET(OUT "${CMAKE_CURRENT_BINARY_DIR}/${FILE}")
    ADD_CUSTOM_COMMAND(
            OUTPUT ${OUT}
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/${FILE}" "${OUT}"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${FILE}"
            COMMENT "Copying proto file ${FILE} to binary dir")
    LIST(APPEND PROTOS "${OUT}")
ENDFOREACH()

ADD_CUSTOM_TARGET("protos" DEPENDS ${PROTOS} )

# define the root, now in the build tree, for later processing
set(PROTO_ROOT "${CMAKE_BINARY_DIR}/${PROJECT_NAME}")

# generate the cpp files from protos
PROTOBUF_GENERATE(CPP PROTO_SRC PROTO_HEADER
        PROTOFILES ${PROTOS}
        PROTOROOT ${PROTO_ROOT}
        ${INCLUDE_ARG}
        DEPENDS "protos" ${UPSTREAM_TARGETS}
        OUTPATH ${CMAKE_CURRENT_BINARY_DIR})

SET(PROTOLIBNAME "protolib")
ADD_LIBRARY(${PROTOLIBNAME} STATIC
        ${PROTO_SRC} ${PROTO_HEADER})

TARGET_LINK_LIBRARIES(${PROTOLIBNAME}
        ${PROTOBUF_LIBRARIES})


## Adding own library for pose interface
########################################

set(src
    net_utils.cc
    remote_interface.cc
    data_receiver.h
)

set(hh
    net_utils.h
    remote_interface.h
)

add_library(rc_dynamics_api_static STATIC ${src})
target_link_libraries(rc_dynamics_api_static ${CPR_LIBRARIES} ${PROTOLIBNAME})

add_library(rc_dynamics_api SHARED ${src})
target_link_libraries(rc_dynamics_api LINK_PRIVATE ${CPR_LIBRARIES} ${PROTOLIBNAME})
set_target_properties(rc_dynamics_api PROPERTIES SOVERSION ${abiversion})

install(TARGETS rc_dynamics_api EXPORT PROJECTTargets COMPONENT bin DESTINATION lib)
install(FILES ${hh} COMPONENT dev DESTINATION include/rc_dynamics_api)
install(FILES ${PROTO_HEADER} COMPONENT dev DESTINATION include/roboception/msgs)