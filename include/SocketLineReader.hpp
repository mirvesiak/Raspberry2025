#pragma once

#include <string>

class SocketLineReader {
    std::string buffer;
    int sockfd;

public:
    SocketLineReader(int fd);
    bool readLine(std::string& out);
};