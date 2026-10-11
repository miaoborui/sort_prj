cmake_minimum_required(VERSION 3.20)

project(sort_visualizer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ftxui CONFIG REQUIRED)

add_library(sort_engine src/sort_engine.cpp)
target_include_directories(sort_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(sort_engine PRIVATE -Wall -Wextra -Wpedantic)

add_executable(sort_visualizer src/main.cpp)
target_link_libraries(sort_visualizer
  PRIVATE
    sort_engine
    ftxui::screen
    ftxui::dom
    ftxui::component
)
target_compile_options(sort_visualizer PRIVATE -Wall -Wextra -Wpedantic)

include(CTest)
if(BUILD_TESTING)
  add_executable(sort_engine_test tests/sort_engine_test.cpp)
  target_link_libraries(sort_engine_test PRIVATE sort_engine)
  target_compile_options(sort_engine_test PRIVATE -Wall -Wextra -Wpedantic)
  add_test(NAME sort_engine_test COMMAND sort_engine_test)
endif()
