import sys
import cv2
import json
import threading
import numpy as np
import websocket

from PyQt5.QtWidgets import (
    QApplication, QLabel, QHBoxLayout, QPushButton,
    QVBoxLayout, QWidget, QLineEdit
)
from PyQt5.QtCore import Qt, QTimer
from PyQt5.QtGui import QImage, QPixmap

# ---- WebSocket control ----
ws = None

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

        # Layout
        layout = QVBoxLayout()
        
        # Video display
        self.video_label = QLabel(self)
        self.video_label.setAlignment(Qt.AlignCenter)
        self.video_label.setStyleSheet("background-color: #a3a3a3")
        layout.addWidget(self.video_label)

        self.control_layout = QHBoxLayout()

        # X Y INPUTS
        self.x_y_box = QVBoxLayout()
        self.input_box_x = QLineEdit()
        self.input_box_x.setPlaceholderText("Enter x pos here...")
        self.x_y_box.addWidget(self.input_box_x)

        self.input_box_y = QLineEdit()
        self.input_box_y.setPlaceholderText("Enter y pos here...")
        self.x_y_box.addWidget(self.input_box_y)

        self.x_y_send_button = QPushButton("Send")
        self.x_y_box.addWidget(self.x_y_send_button)

        self.control_layout.addLayout(self.x_y_box)

        # GRABBER BUTTONS
        self.grabber_box = QHBoxLayout()
        self.release_button = QPushButton("RELEASE")
        self.grab_button = QPushButton("GRAB")
        self.grabber_box.addWidget(self.release_button)
        self.grabber_box.addWidget(self.grab_button)
        
        self.control_layout.addLayout(self.grabber_box)

        layout.addLayout(self.control_layout)

        # OUTPUT COMMAND
        self.output_label = QLabel("Output will appear here")
        layout.addWidget(self.output_label)

        self.setLayout(layout)

        # Event bindings
        self.x_y_send_button.clicked.connect(self.send_coords)
        self.release_button.clicked.connect(lambda: self.send_grabber(False))
        self.grab_button.clicked.connect(lambda: self.send_grabber(True))

        # Start video feed timer
        self.cap = cv2.VideoCapture("http://raspberrypi.local:8080/stream")
        self.timer = QTimer()
        self.timer.timeout.connect(self.update_frame)
        self.timer.start(30)

    def send_coords(self):
        if ws and ws.sock and ws.sock.connected:
            x = self.input_box_x.text()
            y = self.input_box_y.text()
            if x and y:
                message = json.dumps({"type": "coords", "x": float(x), "y": float(y)})
                ws.send(message)
                self.output_label.setText(message)
                self.input_box_x.clear()
                self.input_box_y.clear()
            else:
                self.input_box_x.clear()
                self.input_box_y.clear()

    def send_grabber(self, state):
        if ws and ws.sock and ws.sock.connected:
            message = ""
            if state:
                message = json.dumps({"type": "grip", "state": "on"})
            else:
                message = json.dumps({"type": "grip", "state": "off"})
            ws.send(message)
            self.output_label.setText(message)

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
