cmake_minimum_required(VERSION 3.16)
project(Raspberry2025)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -O2")

# Find packages
find_package(OpenCV REQUIRED)
find_package(CivetWeb)

# ---------- CivetWeb (embedded HTTP server) ----------
#add_library(civetweb STATIC external/civetweb/src/civetweb.c)
#target_include_directories(civetweb PUBLIC external/civetweb)
#target_compile_definitions(civetweb PUBLIC NO_SSL)
#target_link_libraries(civetweb PUBLIC pthread)

# ---------- Your executable ----------
add_executable(Raspberry2025
    src/main.cpp
    src/camera_stream.cpp
)

target_include_directories(Raspberry2025 PRIVATE
    include                       # your own headers
)
target_link_libraries(Raspberry2025
    civetweb
    ${OpenCV_LIBS}
)
