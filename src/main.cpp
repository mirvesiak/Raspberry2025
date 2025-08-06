#include "constants.hpp"
#include "camera_stream.hpp"
#include "motor_control.hpp"

#include <iostream>
#include <signal.h>
#include <thread>
#include <atomic>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstdio>
#include <cctype>  // for std::isprint

std::atomic<bool> go_shutdown{false};

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
    bool ev3_started = start_ev3_script();
    int sockfd = -1;
    std::thread motorThread;
    bool motorThreadStarted = false;

    try {
        if (ev3_started) {
            sockfd = connect_to_ev3(Constants::EV3_IP, Constants::PORT);
            if (sockfd < 0) {
                throw std::runtime_error("Failed to connect to EV3");
            }
        }

        signal(SIGINT, onSignal);
        
        // Start the camera stream server (this might throw)
        start_mjpeg_server(true);

        // Start the EV3 motor thread (if connected)
        if (ev3_started) {
            motorThread = std::thread(motorLoop, sockfd);
            motorThreadStarted = true;
        }

        // Wait for shutdown signal
        while (!go_shutdown.load(std::memory_order_relaxed)) {
            pause();
        }

    } catch (const std::exception& e) {
        std::cerr << "[error] Exception: " << e.what() << '\n';
    }

    // CLEANUP
    stop_mjpeg_server();

    if (motorThreadStarted && motorThread.joinable()) {
        go_shutdown.store(true);  // Ensure motor thread sees the shutdown signal
        motorThread.join();

        if (sockfd >= 0) {
            std::string message = "SHUTDOWN";
            send(sockfd, message.c_str(), message.size(), 0);
            shutdown(sockfd, SHUT_RDWR);
        }
    }

    std::cout << "Shutdown complete.\n";
    return 0;
}
