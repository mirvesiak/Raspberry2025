#pragma once
#include <atomic>

extern std::atomic<int> joystick_angle;
extern std::atomic<int> joystick_distance;

void start_mjpeg_server();
void stop_mjpeg_server();