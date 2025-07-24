#pragma once

namespace Constants {
    constexpr int control_loop_ms = 100; // Control loop interval in milliseconds
    constexpr int control_loop_hz = 1000 / control_loop_ms; // Control loop frequency in Hz

    // Movement constants
    constexpr double SENSITIVITY = 0.01 / control_loop_hz; // Sensitivity for joystick movement (multiplied by joystick distance (0-100))
    constexpr double deadzone_x_left = -7.7;
    constexpr double deadzone_x_right = 7.3;
    constexpr double deadzone_y_top = 7.0;
    constexpr double deadzone_y_bottom = -12.5;

    // IK Solver constants
    constexpr double L1 = 11.3;
    constexpr double L2 = 6.8;
    constexpr double offset = 6.0;
    constexpr double J1_limit = 150.0;
    constexpr double J2_limit = 90.0;

    // EV3 connection constants
    constexpr const char* EV3_IP = "10.42.0.3";
    constexpr int PORT = 1234;
    constexpr const char* EV3_SSH_P1 = "ssh -o ConnectTimeout=5 robot@10.42.0.3 'nohup python3 ";
    constexpr const char* EV3_SSH_P2 = " > /dev/null 2>&1 &'";
    constexpr const char* EV3_SCRIPT = "motor_tests.py";
}
