#include "camera_stream.hpp"
#include <opencv2/opencv.hpp>
#include <civetweb.h>
#include <atomic>
#include <thread>
#include <vector>
#include <string>

// ---- globals kept simple for a minimal demo ----
static cv::VideoCapture cam;
static std::atomic<bool> keep_running{true};
std::atomic<int> joystick_angle{0};
std::atomic<int> joystick_distance{0};
std::atomic<bool> isGrabbing{false};

static void translate_message(const std::string_view msg, int *angle, int *distance) {
    // Translate joystick message to float values
    if (msg.empty()) {
        *angle = 0;
        *distance = 0;
        return;
    }

    const auto pos = msg.find('#');
    try {
        *angle = std::stoi(std::string(msg.substr(0, pos)));
        *distance = std::stoi(std::string(msg.substr(pos + 1)));
    } catch (const std::exception &e) {
        std::cerr << "[error] Failed to parse message: " << msg
                  << " " << e.what() << std::endl;
    }
}

static int wsConnect(const mg_connection*, void*) { return 0; }           // accept all

static int wsMessage(mg_connection *conn, int, char *data,
                      size_t len, void*) {
    std::string_view msg = std::string_view{data, len};
    const char prefix = msg[0];     // Save the first char for switch
    msg.remove_prefix(1);           // Remove it before the switch
    switch (prefix)                 // switch on first char
    {
    case 'M':  // Joystick message
        int angle = 0;
        int distance = 0;
        translate_message(msg, &angle, &distance);
        joystick_angle.store(angle, std::memory_order_relaxed);
        joystick_distance.store(distance, std::memory_order_relaxed);
        break;

    case 'G':  // Switch message
        if (msg == "1") {
            isGrabbing.store(true, std::memory_order_relaxed);
        } else if (msg == "0") {
            isGrabbing.store(false, std::memory_order_relaxed);
        } else {
            std::cerr << "[error] Invalid switch message: " << msg << std::endl;
        }
        break;
    
    default:
        std::cerr << "[warn] Unrecognized message prefix: " << prefix << ", msg: " << msg << std::endl;
        break;
    }
    
    return 1;  // 1 = keep connection open
}

static int streamHandler(struct mg_connection *conn, void * /*cbdata*/)
{
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

void start_mjpeg_server()
{
    // Open camera (PiCam or USB cam). Try 1280×720; fallback if not supported.
    cam.open("/dev/video0", cv::CAP_V4L2);
    if(!cam.isOpened()) { throw std::runtime_error("Camera open failed"); }
    cam.set(cv::CAP_PROP_FRAME_WIDTH,  1280);
    cam.set(cv::CAP_PROP_FRAME_HEIGHT, 720);
    cam.set(cv::CAP_PROP_FPS, 30);

    // CivetWeb config
    const char *options[] = {
        "listening_ports", "8080",
        "document_root",   "/home/pi/git/Raspberry2025/www",   // website path
        "index_files",     "index.html",
        "num_threads",     "6",
        nullptr
    };
    static struct mg_callbacks callbacks;
    struct mg_context *ctx = mg_start(&callbacks, nullptr, options);

    // Register the /stream endpoint
    mg_set_request_handler(ctx, "/stream", streamHandler, nullptr);
    mg_set_websocket_handler(ctx, "/ws", wsConnect, nullptr, wsMessage, nullptr, nullptr);
    std::puts("MJPEG stream running on http://raspberrypi.local:8080/stream");
}

void stop_mjpeg_server() { keep_running = false; }
