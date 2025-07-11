#ifndef SOCKET_LINE_READER_HPP
#define SOCKET_LINE_READER_HPP

#include <string>

class SocketLineReader {
    std::string buffer;
    int sockfd;

public:
    SocketLineReader(int fd);
    bool readLine(std::string& out);
};

#endif // SOCKET_LINE_READER_HPP