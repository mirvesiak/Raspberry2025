#include "camera_stream.hpp"
#include "SocketLineReader.hpp"
#include <iostream>
#include <signal.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstring>      // For memset()
#include <unistd.h>
#include <arpa/inet.h>  // For socket functions

static std::atomic<bool> go_shutdown{false};

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

void motorLoop(int sockfd)
{
    SocketLineReader reader(sockfd);
    std::string line;
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

    while (!go_shutdown.load(std::memory_order_relaxed)) {
        int a = joystick_angle.load(std::memory_order_relaxed);
        int d = joystick_distance.load(std::memory_order_relaxed);
        // send motor command
        std::string message = "MOTOR " + std::to_string(a) + " " + std::to_string(d) + "\n";
        send(sockfd, message.c_str(), message.size(), 0);
        
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