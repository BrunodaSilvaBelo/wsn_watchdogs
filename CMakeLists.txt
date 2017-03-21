cmake_minimum_required( VERSION 3.3 )
project( dog CXX )

set_property( CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release )

set( CMAKE_CXX_STANDARD 14 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

find_package( OpenCV REQUIRED )

set( ${PROJECT_NAME}_MAJOR_VERSION 0 )
set( ${PROJECT_NAME}_MINOR_VERSION 0 )
set( ${PROJECT_NAME}_PATCH_VERSION 1 )
set( ${PROJECT_NAME}_VERSION
  ${${PROJECT_NAME}_MAJOR_VERSION}.${${PROJECT_NAME}_MINOR_VERSION}.${${PROJECT_NAME}_PATCH_VERSION} )

option ( ${PROJECT_NAME}_CONFIGURE_TEST
  "If ON, it will configurate to run tests" OFF )
option ( ${PROJECT_NAME}_USE_GUIDELINES_SUPPORT_LIBRARY
  "If ON, it will configurate to use the c++ standard guidelines" OFF )

if ( ${PROJECT_NAME}_CONFIGURE_TEST
    OR ${PROJECT_NAME}_USE_GUIDELINES_SUPPORT_LIBRARY )
  include(ExternalProject)
  find_package(Git REQUIRED)
endif (  ${PROJECT_NAME}_CONFIGURE_TEST
  OR ${PROJECT_NAME}_USE_GUIDELINES_SUPPORT_LIBRARY )

set( LIB_NAME ${PROJECT_NAME} )
set( LIBRARY_TYPE "STATIC" CACHE STRING "Choose if the library will the static
linked or shared linked" )
set_property( CACHE LIBRARY_TYPE PROPERTY STRINGS STATIC SHARED )
mark_as_advanced( LIBRARY_TYPE )

set( SOURCE ${SOURCE}
  ${PROJECT_SOURCE_DIR}/src/network.cpp)

set( EXTRA_DIRS ${EXTRA_DIRS}
  ${PROJECT_SOURCE_DIR}/src
  "${OpenCV_INCLUDE_DIRS}")

set( EXTRA_LIBS ${EXTRA_LIBS}
  "${OpenCV_LIBS}")

add_library ( ${LIB_NAME}
  ${LIBRARY_TYPE}
  ${SOURCE} )
target_compile_options( ${LIB_NAME} PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/W4> )
target_compile_options( ${LIB_NAME} PRIVATE
  $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -pedantic> )
target_compile_options( ${LIB_NAME} PRIVATE
  $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -pedantic -Weverything
  -Wno-c++98-compat> )

target_include_directories( ${LIB_NAME} PUBLIC
  $<BUILD_INTERFACE:${EXTRA_DIRS}> )
if ( ${PROJECT_NAME}_USE_GUIDELINES_SUPPORT_LIBRARY )
  include( gslconfig )
endif ( ${PROJECT_NAME}_USE_GUIDELINES_SUPPORT_LIBRARY )

target_link_libraries( ${LIB_NAME} ${EXTRA_LIBS} ${CMAKE_THREAD_LIBS_INIT})

set( MAIN_EXECUTABLE ${PROJECT_NAME}_code )
add_executable( ${MAIN_EXECUTABLE} src/main.cpp )
target_link_libraries( ${MAIN_EXECUTABLE} ${LIB_NAME} )

if ( ${PROJECT_NAME}_CONFIGURE_TEST )
  include( testsconfig )
endif ( ${PROJECT_NAME}_CONFIGURE_TEST )

option( ${PROJECT_NAME}_USE_GOOGLE_BENCHMARK
  "If ON, it will use the google microbenchmark" OFF )
if ( ${PROJECT_NAME}_USE_GOOGLE_BENCHMARK )
  include( microbenchconfig )
endif ( ${PROJECT_NAME}_USE_GOOGLE_BENCHMARK )

option( ${PROJECT_NAME}_USE_DOXYGEN "If ON, it will configurate for use doxygen"
  OFF )
if ( ${PROJECT_NAME}_USE_DOXYGEN )
  include( doxygenconfig )
endif ( ${PROJECT_NAME}_USE_DOXYGEN )

configure_file( ${PROJECT_SOURCE_DIR}/src/configuration.h.in
  ${PROJECT_BINARY_DIR}/configuration.h @ONLY )
