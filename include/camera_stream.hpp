#pragma once
#include <nlohmann/json.hpp>
#include <queue>

class InputHandler {
public:
    // Read-only access
    bool readLastJob(nlohmann::json &job);
    void addJob(nlohmann::json job);

private:
    std::queue<nlohmann::json> job_queue;
};

extern InputHandler inputHandler;

// External functions (not related to the state)
void start_mjpeg_server(bool stream);
void stop_mjpeg_server();
