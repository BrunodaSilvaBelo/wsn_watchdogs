find_package( benchmark REQUIRED )
find_package( Threads REQUIRED )
set( BENCHMARK_EXECUTABLE ${PROJECT_NAME}_microbenchmark )
add_executable( ${BENCHMARK_EXECUTABLE} src/bench.cpp )
target_include_directories( ${BENCHMARK_EXECUTABLE} PUBLIC
  $<BUILD_INTERFACE:${benchmark_INCLUDE_DIR}> )
target_link_libraries( ${BENCHMARK_EXECUTABLE} ${CMAKE_THREAD_LIBS_INIT}
  ${benchmark_LIBRARY} ${LIB_NAME} )
