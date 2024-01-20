cmake_minimum_required(VERSION 3.20)

project(
  alive-gcc
  VERSION 0.0.1
  LANGUAGES CXX
)

execute_process(
  COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=plugin
  OUTPUT_VARIABLE pluginpath
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(CMAKE_GENERATOR STREQUAL "Ninja")
  if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(-fdiagnostics-color=always)
  elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options(-fcolor-diagnostics)
  endif()
endif()

# option(SAN "" Off)

# if(SAN)
#   add_compile_options(-fsanitize=address)
#   add_link_options(-fsanitize=address)
# endif()

set(
  sources
  src/bs.cpp
  src/plugin.cpp
  src/llvm_codegen.cpp
  src/simple_gimple_to_bimple_converter.cpp
)
add_library(plugin SHARED ${sources})
message(STATUS "${pluginpath}/include")
target_include_directories(plugin SYSTEM PRIVATE "${pluginpath}/include")
target_compile_features(plugin PUBLIC cxx_std_20)
target_compile_options(
  plugin
  PRIVATE
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Werror=return-type -Wundef>
  # $<$<CXX_COMPILER_ID:GNU>:-Wuseless-cast>
  $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /permissive->
)
target_compile_options(plugin PRIVATE -fno-rtti)

# cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++-11 -GNinja -DCMAKE_EXPORT_COMPILE_COMMANDS=On

# option(ASSERT_USE_MAGIC_ENUM On)

include(FetchContent)

FetchContent_Declare(
    assert
    GIT_REPOSITORY "https://github.com/jeremy-rifkin/libassert"
    GIT_TAG v1.2.2
)
FetchContent_MakeAvailable(assert)
target_link_libraries(plugin PRIVATE assert)

set(BUILD_SHARED_LIBS TRUE)
FetchContent_Declare(
    fmt
    GIT_REPOSITORY "https://github.com/fmtlib/fmt"
    GIT_TAG f5e54359df4c26b6230fc61d38aa294581393084 # 10.1.1
)
FetchContent_MakeAvailable(fmt)
target_link_libraries(plugin PRIVATE fmt)
