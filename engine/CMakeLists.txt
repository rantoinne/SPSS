cmake_minimum_required(VERSION 3.20)
project(SPSSEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(spss_engine STATIC
  src/store/DatasetStore.cpp
  src/store/Output.cpp
  src/transform/Transform.cpp
  src/procedure/Procedure.cpp
  src/procedure/AdvancedProcedures.cpp
  src/io/IO.cpp
  src/capi/capi.cpp
)

target_include_directories(spss_engine
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/procedure
)

add_executable(spss_engine_tests tests/engine_tests.cpp)
target_link_libraries(spss_engine_tests PRIVATE spss_engine)

enable_testing()
add_test(NAME engine_tests COMMAND spss_engine_tests)

add_executable(spss_cli src/cli/main.cpp)
target_link_libraries(spss_cli PRIVATE spss_engine)
