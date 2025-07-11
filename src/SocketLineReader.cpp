#include "SocketLineReader.hpp"
#include <unistd.h>     // for read(), close()
#include <arpa/inet.h>  // for recv()
#include <stdexcept>

SocketLineReader::SocketLineReader(int fd) : sockfd(fd) {}

bool SocketLineReader::readLine(std::string& out) {
    size_t pos;
    while ((pos = buffer.find('\n')) == std::string::npos) {
        char temp[128];
        ssize_t n = recv(sockfd, temp, sizeof(temp), 0);
        if (n <= 0) return false; // Closed or error
        buffer.append(temp, n);
    }

    out = buffer.substr(0, pos);
    buffer.erase(0, pos + 1);
    return true;
}