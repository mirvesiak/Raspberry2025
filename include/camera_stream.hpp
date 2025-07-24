#pragma once
#include <atomic>

class InputHandler {
public:
    // Read-only access
    int getJoystickAngle() const;
    int getJoystickDistance() const;
    bool getIsGrabbing() const;

    // Called only by camera.cpp to update values
    void updateJoystick(int angle, int distance);
    void setGrabbing(bool grabbing);

private:
    std::atomic<int> joystick_angle{0};
    std::atomic<int> joystick_distance{0};
    std::atomic<bool> isGrabbing{false};
};

extern InputHandler InputHandler;

// External functions (not related to the state)
void start_mjpeg_server(bool stream);
void stop_mjpeg_server();
