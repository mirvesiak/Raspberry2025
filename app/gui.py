import cv2
import threading
from time import sleep
from PyQt5.QtWidgets import (
    QWidget, QLabel, QVBoxLayout, QHBoxLayout,
    QPushButton, QLineEdit, QSizePolicy
)
from PyQt5.QtCore import Qt, QTimer
from PyQt5.QtGui import QImage, QPixmap
import os
from datetime import datetime

from control import send_coords, send_grabber
# from signals import app_signals
from vision import Vision
from video_stream import MJPEGStreamReader
from sorting import SortingManager
from sorting import Colors, CilinderState


GREY = (200, 200, 200)
BLACK = (50, 50, 50)
YELLOW = (0, 255, 255)
GREEN = (0, 255, 0)
RED = (0, 0, 255)

class RobotApp(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Robot Controller")
        self.setGeometry(100, 100, 900, 700)

        # --- GUI Layout ---
        layout = QVBoxLayout()
        self.video_label = QLabel(alignment=Qt.AlignCenter)
        self.video_label.setStyleSheet("background-color: #a3a3a3")
        self.video_label.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.video_label.setMinimumSize(1, 1)
        layout.addWidget(self.video_label, stretch=1)

        self.status_dot = QLabel(self.video_label)
        self.status_dot.setFixedSize(20, 20)
        self.status_dot.move(10, 10)
        self.status_dot.setStyleSheet("background-color: red; border-radius: 10px;")
        self.status_dot.raise_()

        controls_container = QWidget()
        controls_container_layout = QVBoxLayout()
        controls_container_layout.setContentsMargins(0, 0, 0, 0)
        controls_container.setLayout(controls_container_layout)

        # Controls (XY + Grabber)
        controls = QHBoxLayout()

        xy_box = QVBoxLayout()
        self.input_box_x = QLineEdit(placeholderText="Enter x pos here...")
        self.input_box_y = QLineEdit(placeholderText="Enter y pos here...")
        send_btn = QPushButton("Send", clicked=self.handle_send_coords)
        xy_box.addWidget(self.input_box_x)
        xy_box.addWidget(self.input_box_y)
        xy_box.addWidget(send_btn)

        grabber_box = QHBoxLayout()
        grabber_box.addWidget(QPushButton("RELEASE", clicked=lambda: self.handle_grabber(False)))
        grabber_box.addWidget(QPushButton("GRAB", clicked=lambda: self.handle_grabber(True)))

        controls.addLayout(xy_box)
        controls.addLayout(grabber_box)

        controls_container_layout.addLayout(controls)

        # Output label
        self.output_label = QLabel("Output will appear here")
        controls_container_layout.addWidget(self.output_label)

        # Debug Controls
        debug_controls = QHBoxLayout()
        debug_controls.addWidget(QPushButton("Capture Image", clicked=self.capture_image))
        # debug_controls.addWidget(QPushButton("Update BG", clicked=self.update_background))
        debug_controls.addWidget(QPushButton("Detect Objects", clicked=self.detect_objects))
        controls_container_layout.addLayout(debug_controls)

        # Fix total height of controls container
        controls_container.setFixedHeight(160)
        layout.addWidget(controls_container, stretch=0)

        self.setLayout(layout)

        # --- Vision Setup ---
        self.vision = Vision()
        self.dropped_frames = 0
        self.latest_frame = None
        self.new_frame = False
        self._frame_lock = threading.Lock()
        self._vision_thread_running = True
        threading.Thread(target=self.run_vision_loop, daemon=True).start()

        # --- Video Stream Setup ---
        self.stream_reader = MJPEGStreamReader("http://192.168.0.202:8080/stream")
        self.stream_reader.start()

        self.timer = QTimer()
        self.timer.timeout.connect(self.update_frame)
        self.timer.start(30)

    # --- GUI Actions ---
    def handle_send_coords(self):
        x = self.input_box_x.text()
        y = self.input_box_y.text()
        if x and y:
            response = send_coords(x, y)
            self.output_label.setText(response)
            self.input_box_x.clear()
            self.input_box_y.clear()

    def handle_grabber(self, state):
        response = send_grabber(state)
        self.output_label.setText(response)

    def capture_image(self):
        with self._frame_lock:
            if self.latest_frame is None:
                print("[WARN] No frame to capture yet.")
                return
            frame_to_save = self.latest_frame.copy()

        os.makedirs("captures", exist_ok=True)
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        path = f"captures/capture_{timestamp}.jpg"
        cv2.imwrite(path, frame_to_save)
        print(f"[INFO] Saved: {path}")

    def detect_objects(self):
        with self._frame_lock:
            if self.latest_frame is None:
                print("[ERROR] No frame available for object detection.")
                return
            frame_to_detect = self.latest_frame.copy()

        self.vision.detect_objects_rgb(frame_to_detect)

    # --- Frame Processing ---
    def update_frame(self):
        frame = self.stream_reader.get_frame()
        if frame is None:
            return

        # Update shared latest frame safely
        with self._frame_lock:
            self.latest_frame = frame
            self.new_frame = True
        self.set_status_dot(self.vision.isCalibrated())

        # Display in GUI
        # --- Prepare display frame ---
        display_scale = 0.5  # scale factor for smoother GUI
        display_frame = cv2.resize(frame, (0, 0), fx=display_scale, fy=display_scale)

        # Draw detected cylinders
        for obj in self.vision.present_objects:
            x, y, w, h = obj.bbox
            # Scale rectangle for display
            x_disp = int(x * display_scale)
            y_disp = int(y * display_scale)
            w_disp = int(w * display_scale)
            h_disp = int(h * display_scale)

            # Determine rectangle color
            color = GREY if obj.color == Colors.GREY else BLACK
            if not obj.visible:
                color = YELLOW
            if obj.state == CilinderState.SORTED:
                color = GREEN
            elif obj.state == CilinderState.UNREACHABLE:
                color = RED

            # Draw rectangle and reference pixel
            cv2.rectangle(display_frame, (x_disp, y_disp), (x_disp + w_disp, y_disp + h_disp), color, 2)

        rgb = cv2.cvtColor(display_frame, cv2.COLOR_BGR2RGB)
        h, w, ch = rgb.shape
        qimg = QImage(rgb.data, w, h, ch * w, QImage.Format_RGB888)

        scaled = QPixmap.fromImage(qimg).scaled(
            self.video_label.size(), Qt.KeepAspectRatio, Qt.SmoothTransformation
        )
        self.video_label.setPixmap(scaled)

    def run_vision_loop(self):
        while self._vision_thread_running:
            frame_to_process = None
            with self._frame_lock:
                if self.new_frame and self.latest_frame is not None:
                    frame_to_process = self.latest_frame.copy()
                    self.new_frame = False

            if frame_to_process is not None:
                self.vision.update(frame_to_process)
            else:
                sleep(0.01)

    def set_status_dot(self, status: bool):
        color = "green" if status else "red"
        self.status_dot.setStyleSheet(f"background-color: {color}; border-radius: 10px;")

    def closeEvent(self, event):
        self._vision_thread_running = False
        self.stream_reader.stop()
        from control import close_ws
        close_ws()
        send_coords(6, 18.1)
        event.accept()
