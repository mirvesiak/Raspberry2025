from enum import Enum
from control import send_coords, send_grabber
from signals import signal_queue
from time import sleep
import threading


UNSEEN_THRESHOLD = 10
SEEN_THRESHOLD = 3

class Colors(Enum):
    GREY = 0
    BLACK = 1

class CilinderState(Enum):
    UNSORTED = 0
    SORTED = 1
    UNREACHABLE = 2

class Cilinder:
    def __init__(self, color: Colors, world_x, world_y, bbox, state = CilinderState.UNSORTED, sorted_spot_index=None, visible=True):
        self.color = color
        self.world_x = world_x
        self.world_y = world_y
        self.bbox = bbox
        self.state = state
        self.sorted_spot_index = sorted_spot_index
        self.visible = visible
        self.seen_for = 0       # how many freames untill considered visible
        self.not_seen_for = 0   # how many frames since last seen
    
    def seen(self):
        self.not_seen_for = 0
        self.seen_for += 1
        if self.seen_for >= SEEN_THRESHOLD:
            self.visible = True
            self.seen_for = SEEN_THRESHOLD

    def not_seen(self):
        self.not_seen_for += 1
        self.visible = False
        if self.not_seen_for >= UNSEEN_THRESHOLD:
            print(f"[INFO] {self.color} cilinder at ({self.world_x:.2f}, {self.world_y:.2f}) not seen for {self.not_seen_for} frames, removing from list.")
            return True

    def __str__(self):
        return f"{self.color.name} {self.state.name} at ({self.world_x:.2f}, {self.world_y:.2f})"
    
    def __eq__(self, other):
        if not isinstance(other, Cilinder):
            return NotImplemented
        return (self.color == other.color and
                abs(self.world_x - other.world_x) < 1 and
                abs(self.world_y - other.world_y) < 1)

class RobotState(Enum):
    IDLE = 0
    MOVING = 1
    GRABBING = 2
    MOVING_TO_RELEASE = 3
    RELEASING = 4

class SortingManager:
    def __init__(self, vision):
        self.state = RobotState.IDLE
        self.idle_position = True
        self.vision = vision
        self.object_to_sort: Cilinder = None
        threading.Thread(target=self.sorting_loop, daemon=True).start()

    def sorting_null_object(self):
        if not self.object_to_sort:
            # print("[ERR] No object to sort, resetting robot.")
            self.reset_robot()
            return True
        return False

    def state_update(self):
        with self.vision._lock:
            if self.state == RobotState.IDLE:
                # print("IDLING")
                self.object_to_sort = self.vision.get_object_to_sort()
                if self.sorting_null_object():
                    return False
                print(f"Object to sort: {self.object_to_sort}")
                # self.next_state()
            elif self.state == RobotState.MOVING:
                # print("MOVING")
                if self.sorting_null_object():
                    return False
                send_coords(self.object_to_sort.world_x, self.object_to_sort.world_y)
                # next state will be handled by WebSocket message
            elif self.state == RobotState.GRABBING:
                # print("GRABBING")
                if self.sorting_null_object():
                    return False
                send_grabber(True)
                # next state will be handled by WebSocket message
            elif self.state == RobotState.MOVING_TO_RELEASE:
                # print("MOVING TO RELEASE")
                if self.sorting_null_object():
                    return False
                x, y = self.vision.get_empty_spot(self.object_to_sort.color)
                if x is None or y is None:
                    self.next_state()  # skip to RELEASING to drop the object
                    return False
                send_coords(x, y)
                # next state will be handled by WebSocket message
            elif self.state == RobotState.RELEASING:
                # print("RELEASING")
                if self.sorting_null_object():
                    return False
                send_grabber(False)
                # next state will be handled by WebSocket message
            else:
                print(f"Unknown state: {self.state}")
                return False
        return True

    def next_state(self):
        if self.state == RobotState.IDLE:
            self.state = RobotState.MOVING
            self.idle_position = False
        elif self.state == RobotState.MOVING:
            self.state = RobotState.GRABBING
        elif self.state == RobotState.GRABBING:
            self.state = RobotState.MOVING_TO_RELEASE
        elif self.state == RobotState.MOVING_TO_RELEASE:
            self.state = RobotState.RELEASING
        elif self.state == RobotState.RELEASING:
            self.state = RobotState.IDLE
        self.state_update()

    def reset_robot(self):
        if not self.idle_position:
            send_coords(6, 18.1)
            signal_queue.get()
            send_grabber(False)
            signal_queue.get()
            self.state = RobotState.IDLE
            self.idle_position = True
        else:
            # print("[INFO] Robot already in IDLE position, no reset needed.")
            pass

    def sorting_loop(self):
        print("[INFO] Sorting loop started")
        while True:
            if self.state == RobotState.IDLE:
                if self.state_update():
                    self.next_state()
                else:
                    sleep(1)
            else:
                signal = signal_queue.get()
                if signal == "CMP":
                    # print(f"Done with {self.state.name}")
                    self.next_state()
                elif signal == "UNR":
                    with self.vision._lock:
                        if self.object_to_sort:
                            print(f"[ERR] Cilinder {self.object_to_sort} is unreachable, setting state to UNREACHABLE")
                            self.object_to_sort.state = CilinderState.UNREACHABLE
                    self.reset_robot()
        