#include "include/camera_stream.hpp"
#include <iostream>

int main()
{
    try {
        start_mjpeg_server();

        // … your motor‑control + WebSocket loop here …
        char condition = 'y';
        do
        {
            std::cout << "MJPEG server running. Press 'n' to stop: ";
            std::cin >> condition;
        } while (condition != 'n' && condition != 'N');
        
        // On shutdown:
        stop_mjpeg_server();
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}