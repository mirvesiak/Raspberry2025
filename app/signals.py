import queue
import threading

ws_connected_event = threading.Event()

signal_queue = queue.Queue()
def send_signal(signal):
    signal_queue.put(signal)
