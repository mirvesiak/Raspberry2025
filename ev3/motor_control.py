import socket
from ev3dev2.motor import Motor, OUTPUT_A, OUTPUT_B, OUTPUT_C, SpeedNativeUnits
from time import time, sleep
from math import pi, sin

s = socket.socket()
s.bind(('0.0.0.0', 1234))
s.listen(5)
conn, _ = s.accept()

def clip(value, min_value, max_value):
    return max(min(value, max_value), min_value)

def smooth_lerp(progress):
    progress = -pi/2 + progress * pi
    return sin(progress) / 2 + 0.5

class PID:
    def __init__(self, kp, ki, kd):
        self.kp = kp
        self.ki = ki
        self.kd = kd
        self.prev_error = 0
        self.integral = 0
        self.prev_time = time() * 1000  # Store time in milliseconds

    def compute(self, setpoint, measured_value):
        time_change = (time() * 1000) - self.prev_time  # Time in milliseconds
        error = setpoint - measured_value
        self.integral += error * time_change
        derivative = (error - self.prev_error) / time_change
        output = (self.kp * error) + (self.ki * self.integral) + (self.kd * derivative)

        self.prev_error = error
        self.prev_time = time() * 1000  # Update previous time
        return output

class VirtualMotor:
    def __init__(self, port, max_speed, ratio, polarity, max_output=100):
        self.motor = Motor(port)
        self.motor.stop_action = 'brake'
        self.motor.polarity = polarity
        self.max_speed = max_speed
        self.max_output = max_output
        self.ratio = ratio
        self.pid = PID(1.1, 0, 0.1)
        self.update_frequency = 100 # Hz

        # in encoder counts
        self.reference_position = self.motor.position
        self.target_position = 0
        
        self.speed = 0
        
        filename = self.motor.address[10:] + "_log.csv"
        self.log_file = open(filename, "w")
        self.log_file.write("Time(ms),Setpoint,Position,PID_Output\n")
    
    def reset_position(self):
        self.reference_position = self.motor.position

    def get_position(self):
        return self.motor.position - self.reference_position

    def set_speed(self, speed):
        self.speed = speed
    
    def on(self):
        if self.speed != 0:
            self.motor.on(self.speed)
        else:
            self.stop()
    
    def stop(self):
        self.speed = 0
        self.motor.stop()

    def get_link_position(self):
        return self.get_position() / self.ratio

    def set_link_target(self, target):  # target in degrees
        self.target_position = target * self.ratio

    def update(self):
        pos = self.get_position()
        output = self.pid.compute(self.target_position, pos)
        clipped_output = clip(output, -self.max_output, self.max_output)
        self.set_speed(clipped_output)
        self.on()
        
        # Logging
        current_time = int(time() * 1000)
        setpoint_deg = self.target_position / self.ratio
        position_deg = self.get_link_position()
        log_line = "{},{:.2f},{:.2f},{:.2f}\n".format(current_time, setpoint_deg, position_deg, clipped_output)
        print(log_line.strip())
        self.log_file.write(log_line)

class Grabber:
    def __init__(self, port, polarity):
        self.motor = Motor(port)
        self.motor.stop_action = 'brake'
        self.motor.polarity = polarity
        
        self.grab_index = 0
        filename = self.motor.address[10:] + "_grab_log.csv"
        self.grab_log_file = open(filename, "w")
        self.grab_log_file.write("Grab_Index,Speed\n")
        
        self.release_index = 0
        filename = self.motor.address[10:] + "_release_log.csv"
        self.release_log_file = open(filename, "w")
        self.release_log_file.write("Release_Index,Speed\n")

    def grab(self):
        self.grab_index += 1
        self.motor.duty_cycle_sp = 40  # Set to a safe duty cycle
        self.motor.run_direct()

        prev_speed = None
        while True:
            sleep(0.1)
            log_line = "{},{}\n".format(self.grab_index, self.motor.speed)
            self.grab_log_file.write(log_line)

            speed = abs(self.motor.speed)
            if (prev_speed is not None and prev_speed - speed > 60) or speed < 230:
                self.motor.stop()
                break
            prev_speed = speed
        sleep(0.2)

    def release(self):
        self.release_index += 1
        self.motor.duty_cycle_sp = -40  # Set to a safe duty cycle
        self.motor.run_direct()

        prev_speed = None
        while True:
            sleep(0.1)
            log_line = "{},{}\n".format(self.release_index, self.motor.speed)
            self.release_log_file.write(log_line)

            speed = abs(self.motor.speed)
            if (prev_speed is not None and prev_speed - speed > 25) or speed < 210:
                self.motor.stop()
                break
            prev_speed = speed
        sleep(0.2)


    def relax(self):
        self.release()
        self.motor.on_for_rotations(35, 1)
        sleep(0.5)

    def on(self, speed):
        self.motor.on(SpeedNativeUnits(speed))

    def stop(self):
        self.motor.stop()


def lerp_both_motors(m1, m2, angle1, angle2, m3):
    angle2 += angle1 # m2 is rotationally linked to m1
    start1 = m1.get_link_position()
    start2 = m2.get_link_position()
    
    distance1 = angle1 - start1
    distance2 = angle2 - start2

    start_time = time()
    duration = max(abs(distance1) / m1.max_speed, abs(distance2) / m2.max_speed)
    while time() <= start_time + duration:
        progress = (time() - start_time) / duration
        progress = smooth_lerp(progress)
        
        # Interpolated targets
        target1 = start1 + distance1 * progress
        target2 = start2 + distance2 * progress
        
        # Set and update
        m1.set_link_target(target1)
        m2.set_link_target(target2)
        
        m1.update()
        m2.update()
        m3.on(m2.speed / m2.ratio)
        sleep(1/m1.update_frequency)
    m1.stop()
    m2.stop()
    m3.stop()
    sleep(0.2)

def simple_test():
    p1 = [30, -100, 0]
    p2 = [20, -70, 0]
    for i in range(len(p1)):
        lerp_both_motors(m1, m2, p1[i], p2[i], m3)
        
        sleep(2)
        if i == 0:
            m3.grab()
        elif i == 1:
            m3.release()

conn.sendall(b"Motor Control Starting\n")
conn_file = conn.makefile('r', encoding='utf-8')

m1 = VirtualMotor(OUTPUT_A, max_speed=50, ratio=7, polarity='inversed')
m2 = VirtualMotor(OUTPUT_B, max_speed=70, ratio=5, polarity='inversed')
m3 = Grabber(OUTPUT_C, 'normal')

conn.sendall(b"Resetting grabber\n")
m3.release()

conn.sendall(b"RDY\n")

try:
    while True:
        cmd = conn_file.readline().strip()

        if cmd.startswith('MOTOR'):
            _, outA, outB = cmd.split()
            outA = float(outA)
            outB = float(outB)

            lerp_both_motors(m1, m2, outA, outB, m3)
            conn.sendall(b"OK\n")

        elif cmd.startswith('GRABBER'):
            _, state = cmd.split()
            if state == "on":
                m3.grab()
                conn.sendall(b"OK\n")
            elif state == "off":
                m3.release()
                conn.sendall(b"OK\n")
            else:
                conn.sendall(b"Wrong grabber state\n")


        elif cmd == 'SHUTDOWN':
            lerp_both_motors(m1, m2, 0, 0, m3)
            # Stop the motor
            m1.motor.off()
            m2.motor.off()
            m3.relax()
            m3.motor.off()
            break
except Exception as e:
    error_msg = "ERROR: " + str(e) + "\n"
    conn.sendall(error_msg.encode())

finally:
    conn.sendall(b"Shutting down EV3.\n")
    conn.close()
    s.close()
