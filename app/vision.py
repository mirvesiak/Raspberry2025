import cv2
import numpy as np
import threading
from sorting import SortingManager, Cilinder, CilinderState, Colors, RobotState

ARUCO_DICT = cv2.aruco.getPredefinedDictionary(cv2.aruco.DICT_4X4_50)
ARUCO_PARAMS = cv2.aruco.DetectorParameters()
DETECTOR = cv2.aruco.ArucoDetector(ARUCO_DICT, ARUCO_PARAMS)

MIN_AXIS = 40
MAX_AXIS = 85
ASPECT_RATIO_MIN = 0.5
ASPECT_RATIO_MAX = 2.0
MERGE_DISTANCE = 40
BLACK_THRESHOLD = -110
GREY_THRESHOLD = -50

X_L = -10
X_R = 10
Y_B = -10
Y_T = 10

SORTING = True  # Set to True to enable sorting logic

# Real-world positions in cm
MARKER_WORLD_COORDS = {
    0: (-10, 10),
    1: (10, 10),
    2: (-10, -10),
    3: (10, -10),
}

GREY_TARGETS = {
    0: (-18, -6),
    1: (-14, -11),
    2: (-8, -16)
}

BLACK_TARGETS = {
    0: (18, -6),
    1: (14, -11),
    2: (9, -13)
}

TARGETS = [GREY_TARGETS, BLACK_TARGETS]

def detect_corners(frame):
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    corners, ids, _ = DETECTOR.detectMarkers(gray)
    return corners, ids

class Vision:
    def __init__(self):
        self.H_mat = None
        self.is_H_fresh = False
        self.background_frame = None
        # self.background_frame_small = None
        self.update_background()  # Load initial background
        self.frame_index = 0
        # Sorting
        self._lock = threading.Lock()
        self.present_objects = []
        if SORTING:
            self.sorting_manager = SortingManager(self)

    def isCalibrated(self):
        return self.H_mat is not None and self.is_H_fresh
    
    def isHSet(self):
        return self.H_mat is not None

    def pixel_to_world(self, x_pixel, y_pixel):
        pixel = np.array([x_pixel, y_pixel, 1.0])
        world = self.H_mat @ pixel
        world = world / world[2]
        return world[0], world[1]

    def calibrate(self, frame):
        corners, ids = detect_corners(frame)

        image_points = []
        world_points = []

        if ids is None:
            self.is_H_fresh = False
            return False

        for i, id_ in enumerate(ids.flatten()):
            if id_ in MARKER_WORLD_COORDS:
                c = corners[i][0]
                center = c.mean(axis=0)
                image_points.append(center)
                world_points.append(MARKER_WORLD_COORDS[id_])

        if len(image_points) >= 4:
            image_pts = np.array(image_points, dtype="float32")
            world_pts = np.array(world_points, dtype="float32")
            H, _ = cv2.findHomography(image_pts, world_pts)
            self.H_mat = H
            self.is_H_fresh = True
            return True

        self.is_H_fresh = False
        return False

    def detect_objects_rgb(self, frame):
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

        # Background subtraction
        diff = cv2.absdiff(gray, self.background_frame)
        _, thresh = cv2.threshold(diff, 30, 255, cv2.THRESH_BINARY)
        thresh = cv2.morphologyEx(thresh, cv2.MORPH_OPEN, np.ones((3, 3), np.uint8))

        # cv2.imshow("Diff", diff)
        # cv2.imshow("Thresh", thresh)
        # cv2.waitKey(1)

        # Find contours
        contours, _ = cv2.findContours(thresh, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

        detected = []
        for cnt in contours:
            if len(cnt) < 5:
                continue

            ellipse = cv2.fitEllipse(cnt)
            (x, y), (MA, ma), angle = ellipse

            # Filter by size and shape
            if MA < MIN_AXIS or ma < MIN_AXIS or MA > MAX_AXIS or ma > MAX_AXIS:
                # print(f"Skipping contour with MA={MA}, ma={ma} at ({x}, {y}) due to size limits.")
                continue

            aspect_ratio = MA / ma if ma > 0 else 1
            if aspect_ratio < ASPECT_RATIO_MIN or aspect_ratio > ASPECT_RATIO_MAX:
                # print(f"Skipping contour with aspect ratio={aspect_ratio:.2f} at ({x}, {y}) due to aspect ratio limits.")
                continue
            # print(f"Detected contour at ({x}, {y}) with MA={MA}, ma={ma}, aspect_ratio={aspect_ratio:.2f}")
            detected.append(((x, y), (MA, ma), angle, cnt))

        # Merge close contours
        merged = []
        used = set()
        for i in range(len(detected)):
            if i in used:
                continue
            (x1, y1), size1, _, cnt1 = detected[i]
            merged_cnt = cnt1.copy()
            for j in range(i+1, len(detected)):
                if j in used:
                    continue
                (x2, y2), size2, _, cnt2 = detected[j]
                dist = np.linalg.norm([x1 - x2, y1 - y2])
                if dist < MERGE_DISTANCE:
                    used.add(j)
                    merged_cnt = np.vstack((merged_cnt, cnt2))
            used.add(i)
            merged.append(merged_cnt)

        results = []
        # debug_frame = frame.copy()
        for cnt in merged:
            x, y, w, h = cv2.boundingRect(cnt)

            mask = np.zeros_like(gray)
            cv2.drawContours(mask, [cnt], -1, 255, -1)

            cropped_img = gray[y:y+h, x:x+w]
            cropped_bg = self.background_frame[y:y+h, x:x+w]
            cropped_mask = mask[y:y+h, x:x+w]

            object_avg = cv2.mean(cropped_img, mask=cropped_mask)[0]
            background_avg = cv2.mean(cropped_bg, mask=cropped_mask)[0]
            avg_diff = object_avg - background_avg

            # Threshold tuned to your previous value
            if avg_diff < BLACK_THRESHOLD:
                color = Colors.BLACK
            elif avg_diff < GREY_THRESHOLD:
                color = Colors.GREY
            else:
                # print(f"Skipping contour at ({x}, {y}) with avg_diff={avg_diff:.2f} (not black or grey)")
                continue

            center_x = x + w // 2
            center_y = y + h // 2

            # print(f"Detected {color} cylinder at ({center_x}, {center_y}) | "
            #       f"object_avg={object_avg:.2f}, bg_avg={background_avg:.2f}, diff={avg_diff:.2f}")

            # # Draw bounding rectangle
            # cv2.rectangle(debug_frame, (x, y), (x+w, y+h), (0, 255, 0), 2)

            # # Draw the current "reference point" (center)
            # cv2.circle(debug_frame, (center_x, center_y), 4, (0, 0, 255), -1)

            # # Example: If you want to test an offset (say bottom-center of rect)
            # offset_x = x + w // 2
            # offset_y = y + h   # bottom edge
            # cv2.circle(debug_frame, (offset_x, offset_y), 4, (255, 0, 0), -1)

            world_x, world_y = self.pixel_to_world(center_x, center_y)
            if X_L <= world_x <= X_R and Y_B <= world_y <= Y_T:
                print(f"[INFO] Skipping object at ({world_x:.2f}, {world_y:.2f}) too close to robot.")
                continue  # Skip if outside the defined area
            sorted = False
            index = None
            for key, value in TARGETS[color.value].items():
                if abs(world_x - value[0]) < 1.5 and abs(world_y - value[1]) < 1.5:
                    sorted = True
                    index = key
                    break
            results.append(Cilinder(color, world_x, world_y, (x, y, w, h), CilinderState.SORTED if sorted else CilinderState.UNSORTED, index))
        with self._lock:
            for obj in results:
                if obj not in self.present_objects:
                    print(f"[INFO] New object detected: {obj}")
                    self.present_objects.append(obj)
            
            for obj in self.present_objects:
                if obj not in results:
                    if obj.not_seen():
                        # Remove from present objects if not seen for too long
                        self.present_objects.remove(obj)
                        print(f"[INFO] Object removed: {obj}")
                else:
                    obj.seen()

            print(f"[INFO] Detected objects in the latest frame: {', '.join(str(obj) for obj in self.present_objects)}")

            # cv2.imshow("Detections", debug_frame)
            # cv2.waitKey(1)

    def get_object_to_sort(self):
        for obj in self.present_objects:
            if obj.state == CilinderState.UNSORTED and obj.visible:
                return obj
        return None

    def get_empty_spot(self, color: Colors):
        taken_spots = [obj.sorted_spot_index for obj in self.present_objects if obj.state == CilinderState.SORTED and obj.color == color]
        for key, value in TARGETS[color.value].items():
            if key not in taken_spots:
                return value[0], value[1]
        return None, None

    def update_background(self):
        frame = cv2.imread("background.jpg", cv2.IMREAD_GRAYSCALE)
        if frame is None:
            print("[ERROR] Background image not found.")
            return
        # self.background_frame_small = cv2.resize(frame, (0, 0), fx=SCALE_FACTOR, fy=SCALE_FACTOR)
        self.background_frame = frame
        print("[INFO] Background updated.")

    def update(self, frame):
        if SORTING and self.frame_index % 10 == 0 and self.isHSet():
            self.detect_objects_rgb(frame)
        
        if self.frame_index == 30:
            self.frame_index = 0
            self.calibrate(frame)
        
        self.frame_index += 1

