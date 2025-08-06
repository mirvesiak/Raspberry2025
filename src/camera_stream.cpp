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

JobHandler jobHandler;

static mg_connection *ws_client_conn = nullptr;  // WebSocket client connection
static std::mutex ws_conn_mutex;

bool JobHandler::readLastJob(json &job) {
    if (!job_queue.empty()) {
        job = job_queue.front();
        job_queue.pop();
        return true;
    }
    return false;
}

void JobHandler::addJob(json job) {
    job_queue.push(job);
}

static int wsConnect(const mg_connection* conn, void*) {
    std::lock_guard<std::mutex> lock(ws_conn_mutex);
    ws_client_conn = (mg_connection*)conn;
    return 0; // accept all
}       

static void wsClose(const mg_connection* conn, void*) {
    std::lock_guard<std::mutex> lock(ws_conn_mutex);
    if (ws_client_conn == conn) {
        ws_client_conn = nullptr;
    }
}

static int wsMessage(mg_connection *conn, int, char *data, size_t len, void*) {
    std::string msg(data, len);
    std::cout<<"New job: "<< msg << "\n";
    try
    {
        json j = json::parse(msg);
        jobHandler.addJob(j);
    }
    catch(const json::parse_error& e)
    {
        std::cerr << "JSON parse error: " << e.what() << "\n";
    }

    return 1; // keep the connection open
}

void send_ws_message(const std::string& msg) {
    std::lock_guard<std::mutex> lock(ws_conn_mutex);
    if (ws_client_conn != nullptr) {
        mg_websocket_write(ws_client_conn, WEBSOCKET_OPCODE_TEXT, msg.c_str(), msg.size());
    }
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

        const int original_width = frame.cols;
        const int crop_width = static_cast<int>(original_width * 0.70);
        const int x_offset = (original_width - crop_width) / 2;

        cv::Rect roi(x_offset, 0, crop_width, frame.rows);
        cv::Mat cropped = frame(roi);

        // 2. Re‑encode to JPEG (quality=70 → good size/latency compromise)
        jpg.clear();
        cv::imencode(".jpg", cropped, jpg, {cv::IMWRITE_JPEG_QUALITY, 70});

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
    mg_set_websocket_handler(ctx, "/ws", wsConnect, nullptr, wsMessage, wsClose, nullptr);
}

void stop_mjpeg_server() { keep_running = false; }
