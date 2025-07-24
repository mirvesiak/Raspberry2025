#include "camera_stream.hpp"
#include <opencv2/opencv.hpp>
#include <civetweb.h>
#include <nlohmann/json.hpp>
#include <atomic>
#include <thread>
#include <vector>
#include <string>

using json = nlohmann::json;

// ---- globals kept simple for a minimal demo ----
static cv::VideoCapture cam;
static std::atomic<bool> keep_running{true};

InputHandler inputHandler;

int InputHandler::getJoystickAngle() const {
    return joystick_angle.load();
}

int InputHandler::getJoystickDistance() const {
    return joystick_distance.load();
}

bool InputHandler::getIsGrabbing() const {
    return isGrabbing.load();
}

void InputHandler::updateJoystick(int angle, int distance) {
    joystick_angle.store(angle);
    joystick_distance.store(distance);
}

void InputHandler::setGrabbing(bool grabbing) {
    isGrabbing.store(grabbing);
}


static int wsConnect(const mg_connection*, void*) { return 0; }           // accept all

static int wsMessage(mg_connection *conn, int, char *data, size_t len, void*) {
    std::string msg(data, len);
    try {
        json j = json::parse(msg);

        const std::string type = j.at("type");
        if (type == "joystick") {
            int angle = j.at("angle");
            int distance = j.at("distance");
            std::cout << angle << " " << distance << "\n";
            inputHandler.updateJoystick(angle, distance);
        } else if (type == "grip") {
            std::string state = j.at("state");
            if (state == "on") {
                inputHandler.setGrabbing(true);
            } else if (state == "off") {
                inputHandler.setGrabbing(false);
            }
        } else {
            std::cerr << "[warn] Unknown JSON type: " << type << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[error] Invalid JSON: " << e.what() << std::endl;
    }
    return 1; // keep the connection open
}


static int streamHandler(struct mg_connection *conn, void * /*cbdata*/) {
    // 1.  HTTP headers for MJPEG
    mg_printf(conn,
        "HTTP/1.0 200 OK\r\n"
        "Cache-Control: no-cache\r\n"
        "Pragma: no-cache\r\n"
        "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n");
        
    std::vector<uchar> jpg;
    cv::Mat frame;

    while (keep_running.load())
    {
        if (!cam.read(frame))  // grab frame
            continue;

        // 2. Re‑encode to JPEG (quality=70 → good size/latency compromise)
        jpg.clear();
        cv::imencode(".jpg", frame, jpg, {cv::IMWRITE_JPEG_QUALITY, 70});

        // 3. Send multipart boundary + JPEG chunk
        mg_printf(conn,
                  "--frame\r\n"
                  "Content-Type: image/jpeg\r\n"
                  "Content-Length: %zu\r\n\r\n",
                  jpg.size());
        mg_write(conn, jpg.data(), jpg.size());
        mg_printf(conn, "\r\n");

        // 25–30 fps → sleep ≈33 ms
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }
    return 0;  // close connection
}

void start_mjpeg_server(bool stream) {
    if (stream) {
        // Open camera
        cam.open("/dev/video0", cv::CAP_V4L2);
        if(!cam.isOpened()) { throw std::runtime_error("Camera open failed"); }
        cam.set(cv::CAP_PROP_FRAME_WIDTH,  1280);
        cam.set(cv::CAP_PROP_FRAME_HEIGHT, 720);
        cam.set(cv::CAP_PROP_FPS, 30);
    }
    // CivetWeb config
    const char *options[] = {
        "listening_ports", "8080",
        "num_threads",     "6",
        nullptr
    };
    static struct mg_callbacks callbacks;
    struct mg_context *ctx = mg_start(&callbacks, nullptr, options);

    // Register the /stream endpoint
    if (stream) {
        mg_set_request_handler(ctx, "/stream", streamHandler, nullptr);
        std::puts("MJPEG stream running on http://raspberrypi.local:8080/stream");
    }
    mg_set_websocket_handler(ctx, "/ws", wsConnect, nullptr, wsMessage, nullptr, nullptr);
}

void stop_mjpeg_server() { keep_running = false; }
