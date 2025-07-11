#include "camera_stream.hpp"
#include <iostream>
#include <signal.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstring>      // For memset()
#include <unistd.h>     // For close()
#include <arpa/inet.h>  // For socket functions

static std::atomic<bool> go_shutdown{false};

constexpr const char* EV3_IP = "10.42.0.3";
constexpr int PORT = 1234;

void onSignal(int) {
    go_shutdown.store(true, std::memory_order_relaxed);  // async‑signal‑safe
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
        
        if (retry_count >= 5) {
            std::cerr << "Failed to connect to EV3 after 5 attempts\n";
            return -1;
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));  // Retry every second
    }

    return sockfd;
}

void motorLoop(int sockfd)
{
    while (true) {
        char buffer[128] = {0};
        ssize_t bytes = recv(sockfd, buffer, sizeof(buffer)-1, 0);
        if (bytes <= 0) {
            std::cerr << "Failed to receive data from EV3\n";
            return;
        }
        if (strncmp(buffer, "RDY", 3) == 0) {
            break; // Exit loop when EV3 is ready
        }
        std::cout << "EV3: " << buffer;
    }
    std::cout << "EV3 is ready to receive commands. Starting the transmission.\n";

    while (!go_shutdown.load(std::memory_order_relaxed)) {
        int a = joystick_angle.load(std::memory_order_relaxed);
        int d = joystick_distance.load(std::memory_order_relaxed);
        // send motor command
        std::string message = "MOTOR " + std::to_string(a) + " " + std::to_string(d);
        send(sockfd, message.c_str(), message.size(), 0);

        // receive sensor reading
        char buffer[128] = {0};
        ssize_t bytes = recv(sockfd, buffer, sizeof(buffer)-1, 0);
        if (bytes > 0 && buffer[0] != 'O' && buffer[1] != 'K') {
            std::cout << "!!! EV3: " << buffer;
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

        close(sockfd);          // Close the socket connection
        std::cout << "Shutdown complete.\n";

    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}