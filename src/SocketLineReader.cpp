// #include "SocketLineReader.hpp"
// #include <unistd.h>     // for read(), close()
// #include <arpa/inet.h>  // for recv()
// #include <stdexcept>

// SocketLineReader::SocketLineReader(int fd) : sockfd(fd) {}

// bool SocketLineReader::readLine(std::string& out) {
//     size_t pos;
//     while ((pos = buffer.find('\n')) == std::string::npos) {
//         char temp[128];
//         ssize_t n = recv(sockfd, temp, sizeof(temp), 0);
//         if (n <= 0) return false; // Closed or error
//         buffer.append(temp, n);
//     }

//     out = buffer.substr(0, pos);
//     buffer.erase(0, pos + 1);
//     return true;
// }
#include "SocketLineReader.hpp"
#include <unistd.h>     // for read(), close()
#include <arpa/inet.h>  // for recv()
#include <stdexcept>
#include <iostream>
#include <cerrno>
#include <cstring> // for strerror()

SocketLineReader::SocketLineReader(int fd) : sockfd(fd) {}

bool SocketLineReader::readLine(std::string& out) {
    size_t pos;
    while ((pos = buffer.find('\n')) == std::string::npos) {
        char temp[128];
        ssize_t n = recv(sockfd, temp, sizeof(temp), 0);

        if (n == 0) {
            std::cerr << "[SocketLineReader] Connection closed by peer.\n";
            return false;
        }
        if (n < 0) {
            std::cerr << "[SocketLineReader] recv() failed: " << strerror(errno) << "\n";
            return false;
        }

        std::cerr << "[SocketLineReader] Received " << n << " bytes\n";

        // Print the raw bytes received
        std::cerr << "[SocketLineReader] Raw data: ";
        for (ssize_t i = 0; i < n; ++i) {
            if (std::isprint(temp[i]))
                std::cerr << temp[i];
            else if (temp[i] == '\n')
                std::cerr << "\\n";
            else if (temp[i] == '\r')
                std::cerr << "\\r";
            else
                std::cerr << "\\x" << std::hex << (int)(unsigned char)temp[i] << std::dec;
        }
        std::cerr << "\n";

        buffer.append(temp, n);
    }

    out = buffer.substr(0, pos);
    buffer.erase(0, pos + 1);

    std::cerr << "[SocketLineReader] Completed line: \"" << out << "\"\n";
    return true;
}
