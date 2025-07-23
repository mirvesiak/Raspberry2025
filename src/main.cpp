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

static std::atomic<bool> go_shutdown{false};

// Movement constants
constexpr double SENSITIVITY = 0.00055f; // Sensitivity for joystick input (1cm/s)
constexpr double deadzone_x_left = -7.7; // Deadzone for x-axis
constexpr double deadzone_x_right = 7.3; // Deadzone for distance
constexpr double deadzone_y_top = 7.0; // Deadzone for y-axis
constexpr double deadzone_y_bottom = -12.5; // Deadzone for y-axis

// IK Solver constants
constexpr double L1 = 11.3; // Length of the first link
constexpr double L2 = 6.8; // Length of the second link
constexpr double offset = 6.0; //Offset of the EE
constexpr double J1_limit = 150.0; // Joint 1 limit in degrees
constexpr double J2_limit = 90.0; // Joint 2 limit in degrees

// EV3 connection constants
constexpr const char* EV3_IP = "10.42.0.3";
constexpr int PORT = 1234;

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
            std::cerr << "Failed to connect to EV3 after 7 attempts\n";
            return -1;
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));  // Retry every second
    }

    return sockfd;
}

void lineLine(double x1, double y1, double x2, double y2, double x3, double y3, double x4, double y4, double &intersectionX, double &intersectionY) {
  // calculate the distance to intersection point
  double uA = ((x4-x3)*(y1-y3) - (y4-y3)*(x1-x3)) / ((y4-y3)*(x2-x1) - (x4-x3)*(y2-y1));
  double uB = ((x2-x1)*(y1-y3) - (y2-y1)*(x1-x3)) / ((y4-y3)*(x2-x1) - (x4-x3)*(y2-y1));

  // if uA and uB are between 0-1, lines are colliding
  if (uA >= 0 && uA <= 1 && uB >= 0 && uB <= 1) {
    // optionally, draw a circle where the lines meet
    intersectionX = x1 + (uA * (x2-x1));
    intersectionY = y1 + (uA * (y2-y1));
  }
}

void joystick_to_coordinates(int angle, int distance, double& x, double& y) {
    double new_x = x + distance * SENSITIVITY * std::cos(angle * PI / 180.0);
    double new_y = y + distance * SENSITIVITY * std::sin(angle * PI / 180.0);

    // Apply deadzone
    if ((new_x < deadzone_x_left || new_x > deadzone_x_right) && (new_y < deadzone_y_bottom || new_y > deadzone_y_top)) {
        // If outside deadzone, update coordinates
        x = new_x;
        y = new_y;
    } else {
        // If inside deadzone, clamp to the nearest deadzone edge
        lineLine(x, y, new_x, new_y, deadzone_x_left, deadzone_y_top, deadzone_x_right, deadzone_y_top, x, y); // Top edge
        lineLine(x, y, new_x, new_y, deadzone_x_right, deadzone_y_top, deadzone_x_right, deadzone_y_bottom, x, y); // Right edge
        lineLine(x, y, new_x, new_y, deadzone_x_right, deadzone_y_bottom, deadzone_x_left, deadzone_y_bottom, x, y); // Bottom edge
        lineLine(x, y, new_x, new_y, deadzone_x_left, deadzone_y_bottom, deadzone_x_left, deadzone_y_top, x, y); // Left edge
    }
}

void motorLoop(int sockfd)
{
    SocketLineReader reader(sockfd);
    std::string line;
    // Wait for the EV3 to be ready
    while (true) {
        if (!reader.readLine(line)) {
            std::cerr << "Read error.\n";
            return;
        }

        if (line == "RDY") {
            std::cout << "EV3 is ready to receive commands. Starting the transmission.\n";
            break;
        } else {
            std::cout << "EV3: " << line << "\n";
        }
    }

    // Initialize the IK solver
    KSolver kSolver(L1, L2, offset);
    double x = 6.0; // Initial x coordinate
    double y = 18.1; // Initial y coordinate
    double outA = 0.0; // Joint 1 angle
    double outB = 0.0; // Joint 2 angle

    // Motor control loop
    bool last_isGrabbing = isGrabbing.load(std::memory_order_relaxed);
    while (!go_shutdown.load(std::memory_order_relaxed)) {
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
            kSolver.calculateIK(x, y, outA, outB);
            
            // Convert angles to degrees
            outA = outA * 180.0 / PI;
            outB = outB * 180.0 / PI;

            // Clamp angles to limits
            if (outA < -J1_limit) outA = -J1_limit;
            if (outA > J1_limit) outA = J1_limit;
            if (outB < -J2_limit) outB = -J2_limit;
            if (outB > J2_limit) outB = J2_limit;

            // Fix the target coordinates
            kSolver.calculateFK(x, y, outA, outB);

            // send motor command
            std::string message = "MOTOR " + std::to_string(outA) + " " + std::to_string(outB) + "\n";
            send(sockfd, message.c_str(), message.size(), 0);
        }

        if (!reader.readLine(line)) {
            std::cerr << "Read error.\n";
            return;
        }

        if (strncmp(line.c_str(), "OK", 2) != 0) {
            std::cout << "EV3: " << line << "\n";
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 20 Hz
    }
}

int main()
{
    // Start the Python server on the EV3
    std::string ssh_command = "ssh robot@10.42.0.3 'nohup python3 /home/robot/motor_controll.py > /dev/null 2>&1 &'";
    std::cout << "Starting Python server on EV3...\n";
    int result = system(ssh_command.c_str());
    if (result != 0) {
        std::cerr << "Failed to launch server on EV3\n";
        return 1;
    }
    std::cout << "Python server script started\n";
    std::cout << "Waiting 5 seconds for EV3 to be ready...\n";
    std::this_thread::sleep_for(std::chrono::seconds(5));  // Wait for the server to start

    // Connect to the EV3
    int sockfd = connect_to_ev3(EV3_IP, PORT);
    if (sockfd < 0) {
        return 1;  // Connection failed
    }

    // Start the main loop and MJPEG server
    try {
        signal(SIGINT, onSignal);   // kill on Ctrl+C
        start_mjpeg_server();

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