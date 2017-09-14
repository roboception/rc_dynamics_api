set(JSON_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/json/src PARENT_SCOPE)

if(WIN32)
    option(CMAKE_USE_OPENSSL "Use OpenSSL code. Experimental" OFF)
endif()

## disabling tests for cpr as googletests cause some issue in catkin workflow:
#  "CMake Error at cpr/opt/googletest/cmake/internal_utils.cmake:145 (add_library):
#  add_library cannot create target "gtest" because another target with the
#  same name already exists.  The existing target is a shared library created
#  in source directory "/usr/src/gtest".  See documentation for policy CMP0002
#  for more details.
#  Call Stack (most recent call first):
#  cpr/opt/googletest/cmake/internal_utils.cmake:168 (cxx_library_with_type)
#  cpr/opt/googletest/CMakeLists.txt:80 (cxx_library)"
set(BUILD_CPR_TESTS OFF CACHE BOOL "Don't build cpr tests")

# build cpr as shared lib
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# unset version variables
unset(PROJECT_VERSION)
unset(PACKAGE_VERSION)

## check for curl being available in system already
find_package(CURL)
if(CURL_FOUND)
    set(USE_SYSTEM_CURL ON CACHE BOOL "Use the system curl for faster builds")
endif()
add_subdirectory(cpr)

#message(STATUS "1: ${CMAKE_CURRENT_BINARY_DIR} 2: ${LIBRARY_OUTPUT_PATH} 3: ${PROJECT_BINARY_DIR} 4:  ")

# since cpr library isnt installing anything at all we need to manually install
# the shared object
#install(FILES ${CMAKE_BINARY_DIR}/lib/libcpr.so
#        DESTINATION lib
#)

#install(TARGETS cpr EXPORT PROJECTTargets COMPONENT bin DESTINATION lib)