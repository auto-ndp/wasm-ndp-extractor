cmake_minimum_required (VERSION 3.10)
project(QCP_SIMULATOR)

set (CMAKE_CXX_STANDARD 17)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_subdirectory(external/binaryen EXCLUDE_FROM_ALL)

set(wndpe_sources
    src/wndpe/wndpe.cpp
)

set(wndpe_driver_sources
    src/driver/main.cpp
)

add_library(wndpe ${wndpe_sources})
add_library(wndpe::wndpe ALIAS wndpe)
target_include_directories(wndpe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(wndpe PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(wndpe PUBLIC cxx_std_17)
target_link_libraries(wndpe PUBLIC binaryen Threads::Threads)
target_compile_options(wndpe PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -pedantic>
)

add_executable(wndpe_driver ${wndpe_driver_sources})
target_include_directories(wndpe_driver PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(wndpe_driver PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(wndpe_driver PUBLIC cxx_std_17)
target_link_libraries(wndpe_driver PUBLIC wndpe::wndpe Threads::Threads)
target_compile_options(wndpe_driver PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -pedantic>
)