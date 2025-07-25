import sys
import threading
from PyQt5.QtWidgets import QApplication

from gui import RobotApp
from control import start_websocket

if __name__ == "__main__":
    # Start the WebSocket in a background thread
    ws_thread = threading.Thread(target=start_websocket, daemon=True)
    ws_thread.start()

    app = QApplication(sys.argv)
    window = RobotApp()
    window.show()
    sys.exit(app.exec_())