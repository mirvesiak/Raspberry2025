#pragma once

namespace Constants {
    // Movement constants
    constexpr double SENSITIVITY = 0.00055;
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
    constexpr const char* EV3_SSH_P1 = "ssh robot@10.42.0.3 'nohup python3 ";
    constexpr const char* EV3_SSH_P2 = " > /dev/null 2>&1 &'";
    constexpr const char* EV3_SCRIPT = "arm_test.py";
}
