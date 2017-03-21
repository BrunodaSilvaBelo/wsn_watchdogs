ExternalProject_Add(
  gsl
  PREFIX ${CMAKE_BINARY_DIR}/GSL
  GIT_REPOSITORY https://github.com/Microsoft/GSL.git
  TIMEOUT 10
  UPDATE_COMMAND ${GIT_EXECUTABLE} pull
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
  LOG_DOWNLOAD ON
  )

ExternalProject_Get_Property(gsl source_dir)
set(GSL_INCLUDE_DIR ${source_dir}/include CACHE INTERNAL
  "Path to include folder for GSL")

target_include_directories( ${LIB_NAME} PUBLIC
  $<BUILD_INTERFACE:${GSL_INCLUDE_DIR}> )
