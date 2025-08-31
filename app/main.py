import sys
import threading
from PyQt5.QtWidgets import QApplication

from gui import RobotApp
from control import start_websocket
from signals import ws_connected_event

if __name__ == "__main__":
    # Start the WebSocket in a background thread
    ws_thread = threading.Thread(target=start_websocket, daemon=True)
    ws_thread.start()
    try:
    # Wait for WebSocket to connect

        print("Waiting for WebSocket to connect...")
        while True:
            if ws_connected_event.wait(timeout=1):  # blocks here until connection established
                print("WebSocket connected, starting GUI")
                break
    except KeyboardInterrupt:
        print("Interrupted")
        sys.exit(0)

    app = QApplication(sys.argv)
    window = RobotApp()
    window.show()
    sys.exit(app.exec_())
