cmake_minimum_required(VERSION 3.16)
project(Raspberry2025)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -O2")

# Find packages
find_package(OpenCV REQUIRED)
find_package(CivetWeb)
add_subdirectory(/home/pi/external_libs/json ${CMAKE_BINARY_DIR}/external/json)

# ---------- Your executable ----------
add_executable(Raspberry2025
    src/main.cpp
    src/camera_stream.cpp
    src/SocketLineReader.cpp
    src/KSolver.cpp
    src/motor_control.cpp
)

target_include_directories(Raspberry2025 PRIVATE
    include                       # your own headers
)
target_link_libraries(Raspberry2025
    civetweb
    ${OpenCV_LIBS}
    PRIVATE nlohmann_json::nlohmann_json
)
