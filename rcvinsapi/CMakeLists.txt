# Author: Christian Emmerich

project(rcvinsapi CXX)

## Adding cpr and json library.
add_subdirectory(opt)
include_directories(${CPR_INCLUDE_DIRS} ${JSON_INCLUDE_DIRS})

## source files for roboception/msgs
#include_directories(
#        .
#)

set(protosrc
    ../roboception/msgs/pose.pb.cc
    ../roboception/msgs/pose_stamped.pb.cc
    ../roboception/msgs/quaternion.pb.cc
    ../roboception/msgs/time.pb.cc
    ../roboception/msgs/vector3d.pb.cc
)

set(protohh
    ../roboception/msgs/pose.pb.h
    ../roboception/msgs/pose_stamped.pb.h
    ../roboception/msgs/quaternion.pb.h
    ../roboception/msgs/time.pb.h
    ../roboception/msgs/vector3d.pb.h
)

set(src
    net_utils.cc
    pose_interface.cc
)

set(hh
    net_utils.h
    pose_interface.h
)

#add_library(rcvinsapi_static STATIC ${src})
#target_link_libraries(rcvinsapi_static ${CPR_LIBRARIES})

add_library(rcvinsapi SHARED ${src} ${protosrc})
target_link_libraries(rcvinsapi LINK_PRIVATE ${CPR_LIBRARIES})
set_target_properties(rcvinsapi PROPERTIES SOVERSION ${abiversion})

install(TARGETS rcvinsapi EXPORT PROJECTTargets COMPONENT bin DESTINATION lib)
install(FILES ${hh} COMPONENT dev DESTINATION include/rcvinsapi)
install(FILES ${protohh} COMPONENT dev DESTINATION include/roboception/msgs)