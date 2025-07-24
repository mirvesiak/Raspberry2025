#include "constants.hpp"
#include "camera_stream.hpp"
#include "motor_control.hpp"
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

int main()
{
    // Start the EV3 script, if it fails, exit
    bool ev3_started = start_ev3_script();

    if (ev3_started) {
        // Connect to the EV3
        int sockfd = connect_to_ev3(Constants::EV3_IP, Constants::PORT);
        if (sockfd < 0) {
            return 1;  // Connection failed
        }
    }

    // Start the main loop and MJPEG server
    try {
        signal(SIGINT, onSignal);   // kill on Ctrl+C
        start_mjpeg_server(true);
        if (ev3_started) std::thread motorThread(motorLoop, sockfd);
    
        while (!go_shutdown.load(std::memory_order_relaxed)) {
            pause();
        }

        stop_mjpeg_server();    // Stop the MJPEG server
        if (ev3_started) {
            motorThread.join();     // Wait for the motor thread to finish

            std::string message = "SHUTDOWN";
            send(sockfd, message.c_str(), message.size(), 0);

            shutdown(sockfd, SHUT_RDWR);  // Optional but cleaner
        }
        std::cout << "Shutdown complete.\n";

    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}