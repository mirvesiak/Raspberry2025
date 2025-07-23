#pragma once
#include <atomic>

extern std::atomic<int> joystick_angle;
extern std::atomic<int> joystick_distance;
extern std::atomic<bool> isGrabbing;

void start_mjpeg_server(bool stream);
void stop_mjpeg_server();