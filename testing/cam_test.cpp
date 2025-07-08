// File: cam_test.cpp
//
// Build : g++ cam_test.cpp -o cam_test $(pkg-config --cflags --libs opencv4)
//         (If pkg‑config can’t find “opencv4”, try “opencv” instead.)
//
// Run   : ./cam_test
//
// Two files, shot_1.jpg and shot_2.jpg, should appear in the same folder.

#include <opencv2/opencv.hpp>
#include <chrono>
#include <thread>
#include <iostream>

static bool setProp(cv::VideoCapture& c, int prop, double val, const char* name)
{
    if (!c.set(prop, val)) {
        std::cerr << "Could not set " << name << " (prop " << prop << ")\n";
        return false;
    }
    return true;
}

int main() {
    // Open the first video device (usually /dev/video0 or the PiCam).
    // If you have more than one camera, change the index.
    cv::VideoCapture cap("/dev/video0", cv::CAP_V4L2);   // Force V4L2 backend for Raspberry Pi

    if (!cap.isOpened()) {
        std::cerr << "Could not open camera. Check cabling, permissions, or index.\n";
        return 1;
    }

    setProp(cap, cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M','J','P','G'), "FOURCC");
    setProp(cap, cv::CAP_PROP_FRAME_WIDTH,  1280, "width");
    setProp(cap, cv::CAP_PROP_FRAME_HEIGHT,  720, "height");
    setProp(cap, cv::CAP_PROP_FPS,            30, "fps");

    cap.set(cv::CAP_PROP_BUFFERSIZE, 1);

    // Give the sensor a moment to adjust exposure/white‑balance.
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    cv::Mat frame;
    if (!cap.read(frame) || frame.empty()) {
        std::cerr << "Failed to capture first frame.\n";
        return 1;
    }

    if (!cv::imwrite("shot_1.jpg", frame)) {
        std::cerr << "Could not write shot_1.jpg (check filesystem permissions).\n";
        return 1;
    }

    std::cout << "Saved shot_1.jpg\n";

    // Wait 5 seconds.
    std::this_thread::sleep_for(std::chrono::seconds(5));

    if (!cap.read(frame) || frame.empty()) {
        std::cerr << "Failed to capture second frame.\n";
        return 1;
    }

    if (!cv::imwrite("shot_2.jpg", frame)) {
        std::cerr << "Could not write shot_2.jpg.\n";
        return 1;
    }
    std::cout << "Saved shot_2.jpg\n";

    std::cout << "All done! View the two JPEG files to confirm everything works.\n";
    return 0;
}
