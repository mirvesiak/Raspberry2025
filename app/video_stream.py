import requests
import cv2
import numpy as np
from threading import Thread
from queue import Queue

class MJPEGStreamReader(Thread):
    def __init__(self, url, queue_size=2):
        super().__init__()
        self.url = url
        self.buffer = Queue(maxsize=queue_size)
        self.running = True
        self.daemon = True

    def run(self):
        try:
            stream = requests.get(self.url, stream=True)
            bytes_ = b""
            for chunk in stream.iter_content(chunk_size=1024):
                if not self.running:
                    break
                bytes_ += chunk
                a = bytes_.find(b'\xff\xd8')
                b = bytes_.find(b'\xff\xd9')
                if a != -1 and b != -1:
                    jpg = bytes_[a:b+2]
                    bytes_ = bytes_[b+2:]
                    frame = cv2.imdecode(np.frombuffer(jpg, dtype=np.uint8), cv2.IMREAD_COLOR)
                    if frame is not None and not self.buffer.full():
                        self.buffer.put(frame)
        except Exception as e:
            print(f"[ERROR] MJPEG stream error: {e}")

    def get_frame(self):
        if not self.buffer.empty():
            return self.buffer.get()
        return None

    def stop(self):
        self.running = False
