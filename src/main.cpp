#include "camera_stream.hpp"
#include <iostream>
#include <signal.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <atomic>

static std::atomic<bool> shutdown{false};

void onSignal(int) {
    shutdown.store(true, std::memory_order_relaxed);  // async‑signal‑safe
}

void motorLoop()
{
    while (!shutdown.load(std::memory_order_relaxed)) {
        int a = joystick_angle.load(std::memory_order_relaxed);
        int d = joystick_distance.load(std::memory_order_relaxed);
        // send motor command
        socket.send(boost::asio::buffer("MOTOR A 75\n"));

        // receive sensor reading
        char buf[64];
        size_t n = socket.read_some(boost::asio::buffer(buf));
        std::cout << "EV3 says: " << std::string_view(buf, n);

        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 20 Hz
    }
}

int main()
{
    try {
        signal(SIGINT, onSignal);   // kill on Ctrl+C
        start_mjpeg_server();

        std::thread motorThread(motorLoop);
        
        while (!shutdown.load(std::memory_order_relaxed)) {
            pause();
        }

        stop_mjpeg_server();    // Stop the MJPEG server
        motorThread.join();     // Wait for the motor thread to finish
    
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}