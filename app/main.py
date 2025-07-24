import sys
import cv2
import json
import threading
import numpy as np
import websocket

from PyQt5.QtWidgets import (
    QApplication, QLabel, QPushButton, QSlider,
    QVBoxLayout, QHBoxLayout, QWidget
)
from PyQt5.QtCore import Qt, QTimer
from PyQt5.QtGui import QImage, QPixmap

# ---- WebSocket control ----
ws = None

def send_control(angle, distance, grabbing):
    if ws and ws.sock and ws.sock.connected:
        # Send joystick command
        ws.send(json.dumps({
            "type": "joystick",
            "angle": 90,
            "distance": 50
        }))

        # Send grab command
        ws.send(json.dumps({
            "type": "grip",
            "state": "on"
        }))

def start_websocket():
    global ws
    ws = websocket.WebSocketApp(
        "ws://raspberrypi.local:8080/ws",
        on_open=lambda ws: print("[WS] Connected"),
        on_message=lambda ws, msg: print(f"[WS] Message: {msg}"),
        on_error=lambda ws, err: print(f"[WS] Error: {err}"),
        on_close=lambda ws, code, msg: print("[WS] Closed")
    )
    ws.run_forever()

# ---- GUI App ----
class RobotApp(QWidget):
    def __init__(self):
        super().__init__()

        self.setWindowTitle("Robot Controller")
        self.setGeometry(100, 100, 900, 700)

        # Video display
        self.video_label = QLabel(self)
        self.video_label.setAlignment(Qt.AlignCenter)

        # Sliders
        self.angle_slider = QSlider(Qt.Horizontal)
        self.angle_slider.setRange(0, 360)
        self.angle_slider.setValue(90)

        self.distance_slider = QSlider(Qt.Horizontal)
        self.distance_slider.setRange(0, 100)
        self.distance_slider.setValue(50)

        # Buttons
        self.grab_button = QPushButton("Grab")
        self.release_button = QPushButton("Release")

        # Layout
        layout = QVBoxLayout()
        layout.addWidget(self.video_label)

        slider_layout = QHBoxLayout()
        slider_layout.addWidget(QLabel("Angle"))
        slider_layout.addWidget(self.angle_slider)
        slider_layout.addWidget(QLabel("Distance"))
        slider_layout.addWidget(self.distance_slider)
        layout.addLayout(slider_layout)

        button_layout = QHBoxLayout()
        button_layout.addWidget(self.grab_button)
        button_layout.addWidget(self.release_button)
        layout.addLayout(button_layout)

        self.setLayout(layout)

        # Event bindings
        self.angle_slider.valueChanged.connect(self.send_command)
        self.distance_slider.valueChanged.connect(self.send_command)
        self.grab_button.clicked.connect(lambda: send_control(self.angle_slider.value(), self.distance_slider.value(), True))
        self.release_button.clicked.connect(lambda: send_control(self.angle_slider.value(), self.distance_slider.value(), False))

        # Start video feed timer
        self.cap = cv2.VideoCapture("http://raspberrypi.local:8080/stream")
        self.timer = QTimer()
        self.timer.timeout.connect(self.update_frame)
        self.timer.start(30)

    def send_command(self):
        angle = self.angle_slider.value()
        distance = self.distance_slider.value()
        send_control(angle, distance, False)

    def update_frame(self):
        ret, frame = self.cap.read()
        if not ret:
            return
        rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        h, w, ch = rgb.shape
        bytes_per_line = ch * w
        qimg = QImage(rgb.data, w, h, bytes_per_line, QImage.Format_RGB888)
        self.video_label.setPixmap(QPixmap.fromImage(qimg))

    def closeEvent(self, event):
        self.cap.release()
        if ws:
            ws.close()
        event.accept()

# ---- Main ----
if __name__ == "__main__":
    # Start WebSocket thread
    ws_thread = threading.Thread(target=start_websocket, daemon=True)
    ws_thread.start()

    app = QApplication(sys.argv)
    window = RobotApp()
    window.show()
    sys.exit(app.exec_())
