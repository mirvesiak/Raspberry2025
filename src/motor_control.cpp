#include "constants.hpp"
#include "motor_control.hpp"
#include "camera_stream.hpp"
#include "KSolver.hpp"
#include "SocketLineReader.hpp"
#include <nlohmann/json.hpp>

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <arpa/inet.h>  // For socket functions
#include <cstring>      // For memset()
#include <unistd.h> // for close()
#include <cmath>

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

// void motorLoop_contin(int sockfd)
// {
//     using namespace Constants;
//     SocketLineReader reader(sockfd);
//     std::string line;

//     // Initialize the IK solver
//     KSolver kSolver(L1, L2, offset);
//     double x = 6.0; // Initial x coordinate
//     double y = 18.1; // Initial y coordinate
//     double outA = 0.0; // Joint 1 angle
//     double outB = 0.0; // Joint 2 angle

//     // Motor control loop
//     bool last_isGrabbing = inputHandler.getIsGrabbing();
//     while (!go_shutdown.load(std::memory_order_relaxed)) {
//         // get the current time for loop timing
//         auto loop_start = std::chrono::steady_clock::now();

//         double x = inputHandler.getX();
//         double y = inputHandler.getY();
//         bool current_isGrabbing = inputHandler.getIsGrabbing();

//         if (last_isGrabbing != current_isGrabbing) {
//             last_isGrabbing = current_isGrabbing;
//             std::string message = "GRABBER " + std::to_string(current_isGrabbing) + "\n";
//             send(sockfd, message.c_str(), message.size(), 0);
//         } else {
//             // Convert joystick input to coordinates
//             // joystick_to_coordinates(a, d, x, y);

//             // Calculate inverse kinematics
//             bool reachable = kSolver.calculateIK(x, y, outA, outB);
            
//             // Convert angles to degrees
//             outA = outA * 180.0 / PI;
//             outB = outB * 180.0 / PI;

//             // Clamp angles to limits
//             outA = clampAngle(outA, J1_limit, reachable);
//             outB = clampAngle(outB, J2_limit, reachable);

//             // Fix the target coordinates
//             if (!reachable)
//                 kSolver.calculateFK(x, y, outA, outB);
//             // send motor command
//             char buffer[50];
//             std::snprintf(buffer, sizeof(buffer), "MOTOR %.2f %.2f\n", outA, outB); // round to 2 decimal places
//             std::string message(buffer);
//             // std::cout << "Sending command: " << message;
//             send(sockfd, message.c_str(), message.size(), 0);
//         }

//         if (!reader.readLine(line)) {
//             std::cerr << "Read error.\n";
//             return;
//         }

//         if (strncmp(line.c_str(), "OK", 2) != 0) {
//             std::cout << "EV3: " << line << "\n";
//         }

//         auto loop_end = std::chrono::steady_clock::now();
//         auto iteration_duration = std::chrono::duration_cast<std::chrono::milliseconds>(loop_end - loop_start);
//         auto sleep_duration = std::chrono::milliseconds(control_loop_ms) - iteration_duration;

//         if (sleep_duration > std::chrono::milliseconds(0)) {
//             std::this_thread::sleep_for(sleep_duration);
//         } else {
//             // Optional: warn if over time
//             std::cerr << "Warning: Control loop overran by " 
//                     << -sleep_duration.count() << " ms\n";
//         }
//     }
// }

void coordsJobParse(nlohmann::json j, double &x, double &y) {
    x = j['x'];
    y = j['y'];
}

std::string grabJobParse(nlohmann::json j) {
    std::string state = j["state"];
    return std::string("GRABBER ") + state + "\n";
}

void computeAngles(double x, double y, double &outA, double &outB) {
    using namespace Constants;
    // Initialize the IK solver
    KSolver kSolver(L1, L2, offset);

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
}

void motorLoop(int sockfd) {
    using namespace Constants;
    using json = nlohmann::json;

    SocketLineReader reader(sockfd);
    std::string line;
    json j;

    while (!go_shutdown.load(std::memory_order_relaxed)) {
        if (inputHandler.readLastJob(j)) {
            try {
                const std::string type = j.at("type");
                if (type == "coords") {
                    double x = 0.0, y = 0.0;
                    double outA = 0.0, outB = 0.0;
                    coordsJobParse(j, x, y);
                    computeAngles(x, y, outA, outB);
                    // send motor command
                    char buffer[50];
                    std::snprintf(buffer, sizeof(buffer), "MOTOR %.2f %.2f\n", outA, outB); // round to 2 decimal places
                    std::string message(buffer);
                    // std::cout << "Sending command: " << message;
                    send(sockfd, message.c_str(), message.size(), 0);
                } else if (type == "grip") {
                    std::string message = grabJobParse(j);
                    send(sockfd, message.c_str(), message.size(), 0);
                } else {
                    std::cerr << "[warn] Unknown JSON type: " << type << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "[error] Invalid JSON: " << e.what() << std::endl;
            }
        }

        if (!reader.readLine(line)) {
            std::cerr << "Read error in motorLoop.\n";
            return;
        }

        std::cout << "[RECV] Line: '" << line << "'\n";

        if (strncmp(line.c_str(), "OK", 2) != 0) {
            std::cout << "EV3: " << line << "\n";
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
}