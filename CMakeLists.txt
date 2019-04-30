cmake_minimum_required(VERSION 3.0.0)
project(gbjson VERSION 1.2.3)

include_directories(${PROJECT_SOURCE_DIR}/include ${PROJECT_BINARY_DIR}/include)
configure_file(${PROJECT_SOURCE_DIR}/include/gbjson.in.h ${PROJECT_BINARY_DIR}/include/gbjson.h)

add_executable(gb2json gb2json.cpp)
target_compile_features(gb2json PUBLIC cxx_std_14)

add_executable(json2gb json2gb.cpp)
target_compile_features(json2gb PUBLIC cxx_std_14)

add_library(gbjson gbjson.cpp)
target_compile_features(gbjson PUBLIC cxx_std_14)

target_link_libraries(gb2json gbjson)
target_link_libraries(json2gb gbjson)

if(WIN32 OR APPLE)
    target_link_libraries(gbjson)
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(gbjson m)
endif()

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
