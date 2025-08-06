#pragma once
#include <nlohmann/json.hpp>
#include <queue>
#include <mutex>

class JobHandler {
public:
    // Read-only access
    bool readLastJob(nlohmann::json &job);
    void addJob(nlohmann::json job);

private:
    std::queue<nlohmann::json> job_queue;
};

extern JobHandler jobHandler;

// External functions
void start_mjpeg_server(bool stream);
void stop_mjpeg_server();
void send_ws_message(const std::string& msg);
