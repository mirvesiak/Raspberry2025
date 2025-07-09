#include "camera_stream.hpp"
#include <iostream>
#include <signal.h>
#include <unistd.h>
#include <thread>
#include <chrono>

static std::atomic<bool> shutdown{false};

void onSignal(int) {
    shutdown.store(true, std::memory_order_relaxed);  // async‑signal‑safe
}

void motorLoop()
{
    while (!shutdown.load(std::memory_order_relaxed)) {
        float a = angle.load(std::memory_order_relaxed);
        float d = distance.load(std::memory_order_relaxed);

        std::cout << "[motorLoop] angle=" << a
                  << "  dist=" << d << '\n';

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