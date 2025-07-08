#include "include/camera_stream.hpp"
#include <iostream>
#include <signal.h>
#include <unistd.h>

void onSignal(int) {
    stop_mjpeg_server();
    std::exit(0);
}

int main()
{
    try {
        signal(SIGINT, onSignal);   // kill on Ctrl+C
        start_mjpeg_server();
        pause(); 
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}