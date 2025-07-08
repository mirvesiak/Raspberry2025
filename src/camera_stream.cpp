#include "camera_stream.hpp"
#include <opencv2/opencv.hpp>
#include "civetweb.h"
#include <atomic>
#include <thread>
#include <vector>

// ---- globals kept simple for a minimal demo ----
static cv::VideoCapture cam;
static std::atomic<bool> keep_running{true};

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
        "num_threads",     "4",
        nullptr
    };
    static struct mg_callbacks callbacks;
    struct mg_context *ctx = mg_start(&callbacks, nullptr, options);

    // Register the /stream endpoint
    mg_set_request_handler(ctx, "/stream", streamHandler, nullptr);

    std::puts("MJPEG stream running on http://raspberrypi.local:8080/stream");
}

void stop_mjpeg_server() { keep_running = false; }
