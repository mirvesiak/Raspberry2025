#include "constants.hpp"
#include "camera_stream.hpp"
#include "SocketLineReader.hpp"
#include "KSolver.hpp"
#include <iostream>
#include <signal.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstring>      // For memset()
#include <unistd.h>
#include <arpa/inet.h>  // For socket functions
#include <cmath>
#include <cstdio>

static std::atomic<bool> go_shutdown{false};

void onSignal(int) {
    go_shutdown.store(true, std::memory_order_relaxed);  // async‑signal‑safe
}

void print_raw(const char* data, size_t len) {
    std::cout << "Raw message (" << len << " bytes):\n";
    for (size_t i = 0; i < len; ++i) {
        unsigned char c = data[i];

        // Print hex
        std::printf("%02X ", c);

        // Optional: print printable characters to the right
        if ((i + 1) % 16 == 0 || i == len - 1) {
            std::cout << "  ";
            for (size_t j = i - (i % 16); j <= i; ++j) {
                char ch = data[j];
                std::cout << (std::isprint(ch) ? ch : '.');
            }
            std::cout << "\n";
        }
    }
}

bool start_ev3_script() {    
    using namespace Constants;

    // Start the Python script on the EV3
    std::string ssh_command = std::string(EV3_SSH_P1) + EV3_SCRIPT + EV3_SSH_P2;
    
    std::cout << "Starting Python script on EV3...\n";
    int result = system(ssh_command.c_str());
    if (result != 0) {
        std::cerr << "Failed to launch script on EV3\n";
        return false;
    }
    
    std::cout << "Python script started\n";
    std::cout << "Waiting 5 seconds for EV3 to be ready...\n";
    std::this_thread::sleep_for(std::chrono::seconds(5));  // Wait for the server to start
    
    return true;
}

int connect_to_ev3(const char* ip, int port) {
    int sockfd;
    struct sockaddr_in serv_addr;
    
    int retry_count = 0;

    while (true) {
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            std::cerr << "Socket creation failed\n";
            return -1;
        }

        std::memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port);
        inet_pton(AF_INET, ip, &serv_addr.sin_addr);

        std::cout << "Trying to connect to EV3...\n";
        if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == 0) {
            std::cout << "Connected to EV3!\n";
            break;
        }

        close(sockfd);
        std::cerr << "Connection attempt " << ++retry_count << " , retrying...\n";
        
        if (retry_count >= 7) {
            std::cerr << "Failed to connect to EV3 after 7 attempts.\n";
            return -1;
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));  // Retry every second
    }

    SocketLineReader reader(sockfd);
    std::string line;
    // Wait for the EV3 to be ready
    while (true) {
        if (!reader.readLine(line)) {
            std::cerr << "Read error.\n";
            std::cerr << "Failed to initialize EV3 script.\n";
            return -1;
        }

        if (line == "RDY") {
            std::cout << "EV3 is ready to receive commands.\n";
            break;
        } else {
            std::cout << "EV3: " << line << "\n";
        }
    }

    return sockfd;
}

bool lineLine(double x1, double y1, double x2, double y2, double x3, double y3, double x4, double y4, double &intersectionX, double &intersectionY) {
    // calculate the distance to intersection point
    double uA = ((x4-x3)*(y1-y3) - (y4-y3)*(x1-x3)) / ((y4-y3)*(x2-x1) - (x4-x3)*(y2-y1));
    double uB = ((x2-x1)*(y1-y3) - (y2-y1)*(x1-x3)) / ((y4-y3)*(x2-x1) - (x4-x3)*(y2-y1));

    // if uA and uB are between 0-1, lines are colliding
    if (uA >= 0 && uA <= 1 && uB >= 0 && uB <= 1) {
        intersectionX = x1 + (uA * (x2-x1));
        intersectionY = y1 + (uA * (y2-y1));
        return true; // Lines intersect
    }
    return false; // Lines do not intersect
}

float clampAngle(float angle, float limit, bool& reachable) {
    if (angle < -limit) {
        reachable = false;
        return -limit;
    } 
    if (angle > limit) {
        reachable = false;
        return limit;
    }
    return angle;
}

void joystick_to_coordinates(int angle, int distance, double& x, double& y) {
    using namespace Constants;

    // Convert joystick angle and distance to coordinates
    double new_x = x + distance * SENSITIVITY * std::cos(angle * PI / 180.0);
    double new_y = y + distance * SENSITIVITY * std::sin(angle * PI / 180.0);
    std::cout << "(" << x << ", " << y << ") -> (" << new_x << ", " << new_y << ")\n";
    // Apply deadzone
    if (new_x < deadzone_x_left || new_x > deadzone_x_right || new_y < deadzone_y_bottom || new_y > deadzone_y_top) {
        // If outside deadzone, update coordinates
        x = new_x;
        y = new_y;
    } else {
        // If inside deadzone, clamp to the intersected deadzone edge
        if (lineLine(x, y, new_x, new_y, deadzone_x_left, deadzone_y_top, deadzone_x_right, deadzone_y_top, x, y)) return; // Top edge
        if (lineLine(x, y, new_x, new_y, deadzone_x_right, deadzone_y_top, deadzone_x_right, deadzone_y_bottom, x, y)) return; // Right edge
        if (lineLine(x, y, new_x, new_y, deadzone_x_right, deadzone_y_bottom, deadzone_x_left, deadzone_y_bottom, x, y)) return; // Bottom edge
        if (lineLine(x, y, new_x, new_y, deadzone_x_left, deadzone_y_bottom, deadzone_x_left, deadzone_y_top, x, y)) return; // Left edge
    }
}

void motorLoop(int sockfd)
{
    using namespace Constants;
    SocketLineReader reader(sockfd);
    std::string line;

    // Initialize the IK solver
    KSolver kSolver(L1, L2, offset);
    double x = 6.0; // Initial x coordinate
    double y = 18.1; // Initial y coordinate
    double outA = 0.0; // Joint 1 angle
    double outB = 0.0; // Joint 2 angle

    // Motor control loop
    bool last_isGrabbing = isGrabbing.load(std::memory_order_relaxed);
    while (!go_shutdown.load(std::memory_order_relaxed)) {
        // get the current time for loop timing
        auto loop_start = std::chrono::steady_clock::now();

        int a = joystick_angle.load(std::memory_order_relaxed);
        int d = joystick_distance.load(std::memory_order_relaxed);
        bool current_isGrabbing = isGrabbing.load(std::memory_order_relaxed);

        if (last_isGrabbing != current_isGrabbing) {
            last_isGrabbing = current_isGrabbing;
            std::string message = "GRABBER " + std::to_string(current_isGrabbing) + "\n";
            send(sockfd, message.c_str(), message.size(), 0);
        } else {
            // Convert joystick input to coordinates
            joystick_to_coordinates(a, d, x, y);
            // Calculate inverse kinematics
            bool reachable = kSolver.calculateIK(x, y, outA, outB);
            
            // Convert angles to degrees
            outA = outA * 180.0 / PI;
            outB = outB * 180.0 / PI;

            // Clamp angles to limits
            outA = clampAngle(outA, J1_limit, reachable);
            outB = clampAngle(outB, J2_limit, reachable);

            // Fix the target coordinates
            if (!reachable)
                kSolver.calculateFK(x, y, outA, outB);

            // send motor command
            char buffer[50];
            std::snprintf(buffer, sizeof(buffer), "MOTOR %.2f %.2f\n", outA, outB); // round to 2 decimal places
            std::string message(buffer);
            // std::cout << "Sending command: " << message;
            send(sockfd, message.c_str(), message.size(), 0);
        }

        if (!reader.readLine(line)) {
            std::cerr << "Read error.\n";
            return;
        }

        if (strncmp(line.c_str(), "OK", 2) != 0) {
            std::cout << "EV3: " << line << "\n";
        }

        auto loop_end = std::chrono::steady_clock::now();
        auto iteration_duration = std::chrono::duration_cast<std::chrono::milliseconds>(loop_end - loop_start);
        auto sleep_duration = std::chrono::milliseconds(50) - iteration_duration;

        if (sleep_duration > std::chrono::milliseconds(0)) {
            std::this_thread::sleep_for(sleep_duration);
        } else {
            // Optional: warn if over time
            std::cerr << "Warning: Control loop overran by " 
                    << -sleep_duration.count() << " ms\n";
    }
    }
}

int main()
{
    // Start the EV3 script, if it fails, exit
    if (!start_ev3_script()) return 1;

    // Connect to the EV3
    int sockfd = connect_to_ev3(Constants::EV3_IP, Constants::PORT);
    if (sockfd < 0) {
        return 1;  // Connection failed
    }

    // Start the main loop and MJPEG server
    try {
        signal(SIGINT, onSignal);   // kill on Ctrl+C
        start_mjpeg_server(false);

        std::thread motorThread(motorLoop, sockfd);
        
        while (!go_shutdown.load(std::memory_order_relaxed)) {
            pause();
        }

        stop_mjpeg_server();    // Stop the MJPEG server
        motorThread.join();     // Wait for the motor thread to finish

        std::string message = "SHUTDOWN";
        send(sockfd, message.c_str(), message.size(), 0);

        shutdown(sockfd, SHUT_RDWR);  // Optional but cleaner
        std::cout << "Shutdown complete.\n";

    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}