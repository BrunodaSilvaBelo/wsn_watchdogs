ExternalProject_Add(
  catch
  PREFIX ${CMAKE_BINARY_DIR}/catch
  GIT_REPOSITORY https://github.com/philsquared/Catch.git
  TIMEOUT 10
  UPDATE_COMMAND ${GIT_EXECUTABLE} pull
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
  LOG_DOWNLOAD ON
  )

ExternalProject_Get_Property(catch source_dir)
set(CATCH_INCLUDE_DIR ${source_dir}/include CACHE INTERNAL
  "Path to include folder for Catch")

set( TEST_EXECUTABLE ${PROJECT_NAME}_test )
add_executable( ${TEST_EXECUTABLE} src/test.cpp )
target_link_libraries( ${TEST_EXECUTABLE} ${LIB_NAME} )
target_include_directories( ${TEST_EXECUTABLE} PUBLIC
  $<BUILD_INTERFACE:${CATCH_INCLUDE_DIR}> )
add_dependencies( ${TEST_EXECUTABLE} catch )

enable_testing()
add_test( NAME RunTests COMMAND ${TEST_EXECUTABLE} )
