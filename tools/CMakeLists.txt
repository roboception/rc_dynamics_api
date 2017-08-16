
# Author: Heiko Hirschmueller

project(tools CXX)

# build programs

add_executable(vins_stream vins_stream.cc)
include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR}/../rcvinsapi)
target_link_libraries(vins_stream rcvinsapi_static ${PROTOBUF_LIBRARIES})

# install tools

install(TARGETS vins_stream COMPONENT bin DESTINATION bin)
