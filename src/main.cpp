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

void resolvePointAABBCollision(double oldX, double oldY, double& newX, double& newY, double left, double top, double right, double bottom) {
    // Check if point is inside the deadzone
    if (newX <= left || newX >= right || newY <= top || newY >= bottom)
        return; // Already outside — no action needed

    double dx = newX - oldX;
    double dy = newY - oldY;

    // Calculate distances to nearest edges
    double toLeft   = std::abs(newX - left);
    double toRight  = std::abs(newX - right);
    double toTop    = std::abs(newY - top);
    double toBottom = std::abs(newY - bottom);

    // Choose axis of least penetration (least movement needed to escape)
    if (std::abs(dx) > std::abs(dy)) {
        // Horizontal movement dominates
        if (dx > 0)
            newX = left;   // Moved right - push to left edge
        else
            newX = right;  // Moved left - push to right edge
    } else {
        // Vertical movement dominates
        if (dy > 0)
            newY = top;    // Moved down - push to top edge
        else
            newY = bottom; // Moved up - push to bottom edge
    }
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
    // Resolve collision with deadzone
    resolvePointAABBCollision(x, y, new_x, new_y, deadzone_x_left, deadzone_y_top, deadzone_x_right, deadzone_y_bottom);
    x = new_x;
    y = new_y;
}

void setup_joystick_translation(int angle, std::string & message) {
    if (angle < 45) {
        message = "M LEFT\n";
    } else if (angle < 135) {
        message = "M UP\n";
    } else if (angle < 225) {
        message = "M RIGHT\n";
    } else if (angle < 315) {
        message = "M DOWN\n";
    } else {
        message = "M LEFT\n";
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

            auto calculation_duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - loop_start);
            std::cout << "Control loop iteration took " << calculation_duration.count() << " ms\n";
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
        auto sleep_duration = std::chrono::milliseconds(control_loop_ms) - iteration_duration;

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
        start_mjpeg_server(true);

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