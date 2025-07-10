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

void motorLoop(int sockfd)
{
    while (!go_shutdown.load(std::memory_order_relaxed)) {
        int a = joystick_angle.load(std::memory_order_relaxed);
        int d = joystick_distance.load(std::memory_order_relaxed);
        // send motor command
        std::string message = "MOTOR " + std::to_string(a);
        send(sockfd, message.c_str(), message.size(), 0);

        // receive sensor reading
        char buffer[128] = {0};
        ssize_t bytes = recv(sockfd, buffer, sizeof(buffer)-1, 0);
        if (bytes > 0) {
            std::cout << "EV3 response: " << buffer;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 20 Hz
    }
}

int main()
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    sockaddr_in serv_addr;
    std::memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, EV3_IP, &serv_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address/ Address not supported\n";
        return 1;
    }

    std::cout << "Connecting to EV3 at " << EV3_IP << ":" << PORT << "...\n";
    if (connect(sockfd, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        return 1;
    }

    std::cout << "Connected to EV3!\n";


    try {
        signal(SIGINT, onSignal);   // kill on Ctrl+C
        start_mjpeg_server();

        std::thread motorThread(motorLoop(sockfd));
        
        while (!go_shutdown.load(std::memory_order_relaxed)) {
            pause();
        }

        stop_mjpeg_server();    // Stop the MJPEG server
        motorThread.join();     // Wait for the motor thread to finish
        close(sockfd);          // Close the socket connection
        std::cout << "Shutdown complete.\n";

    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}