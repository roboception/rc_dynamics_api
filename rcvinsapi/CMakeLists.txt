# Author: Christian Emmerich

project(rcvinsapi CXX)

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
    pose_interface.cc
)

set(hh
    net_utils.h
    pose_interface.h
)

add_library(rcvinsapi_static STATIC ${src})
target_link_libraries(rcvinsapi_static ${CPR_LIBRARIES} ${PROTOLIBNAME})

add_library(rcvinsapi SHARED ${src})
target_link_libraries(rcvinsapi LINK_PRIVATE ${CPR_LIBRARIES} ${PROTOLIBNAME})
set_target_properties(rcvinsapi PROPERTIES SOVERSION ${abiversion})

install(TARGETS rcvinsapi EXPORT PROJECTTargets COMPONENT bin DESTINATION lib)
install(FILES ${hh} COMPONENT dev DESTINATION include/rcvinsapi)
install(FILES ${PROTO_HEADER} COMPONENT dev DESTINATION include/roboception/msgs)