import socket
from ev3dev2.motor import Motor, OUTPUT_A, OUTPUT_B
from ev3dev2.display import Display
import ev3dev2.fonts as fonts
from math import sin, cos, pi, radians

s = socket.socket()
s.bind(('0.0.0.0', 1234))
s.listen(5)
conn, _ = s.accept()

class PID:
    def __init__(self, Kp, Ki, Kd):
        self.Kp = Kp
        self.Ki = Ki
        self.Kd = Kd
        self.prev_error = 0
        self.integral = 0
    
    def compute(self, setpoint, measured_value):
        error = setpoint - measured_value
        self.integral += error * dt
        derivative = (error - self.prev_error) / dt
        self.prev_error = error

        return self.Kp * error + self.Ki * self.integral + self.Kd * derivative

class VirtualMotor:
    def __init__(self, port, ratio=1.0):
        self.motor = Motor(port)
        self.speed = 0
        self.raw_speed = 0
        self.ratio = ratio    # mortor_speed * ratio = output

    def calculate_speed(self):
        self.speed = self.raw_speed * self.ratio

    def set_speed(self, speed): 
        self.speed = int(speed / self.ratio)

    def add_speed(self, speed):
        self.speed += int(speed / self.ratio)
    
    def on(self):
        if self.speed != 0:
            self.motor.on(self.speed)
        else:
            self.motor.off()

    def off(self):
        self.speed = 0
        self.motor.off()

def run_motors(motors):
    for motor in motors:
        motor.on()

def translate_data(angle, distance):
    # Placeholder for actual motor speed calculation logic
    x = round(cos(radians(angle)) * distance)
    y = round(sin(radians(angle)) * distance)
    if abs(x) < dead_margin:
        x = 0
    if abs(y) < dead_margin:
        y = 0
    return x, y

conn.sendall(b"Motor Control Starting\n")

conn_file = conn.makefile('r', encoding='utf-8')

display = Display()
# motors = [VirtualMotor(OUTPUT_A, ratio=1/5), VirtualMotor(OUTPUT_B, ratio=1/7)]
motorX = Motor(OUTPUT_B)
speedX = 0
motorY = Motor(OUTPUT_A)
speedY = 0
ratio = 5/7  # Ratio of motor speeds

dead_margin = 0.1  # Deadband for speed control

pidX = PID(Kp=9.0, Ki=0.1, Kd=0.05)
pidY = PID(Kp=9.0, Ki=0.1, Kd=0.05)
dt = 0.05   # 50 ms time step


conn.sendall(b"RDY\n")

try:
    while True:
        cmd = conn_file.readline().strip()
        if cmd.startswith('MOTOR'):
            _, angle, distance = cmd.split()
            rsp = "OK " + angle + " " + distance + "\n"
            conn.sendall(rsp.encode())

            display.clear()
            display.draw.text((10,10), str(angle + " " + distance), font=fonts.load('helvB18'))
            display.update()

            angle = int(angle)
            distance = int(distance)

            setpoint_speedX = 0
            setpoint_speedY = 0
            if abs(distance) > dead_margin:
                setpoint_speedX, setpoint_speedY = translate_data(angle, distance)
            
            ctrlX = pidX.compute(setpoint_speedX, speedX)  
            ctrlY = pidY.compute(setpoint_speedY, speedY)
            speedX += ctrlX * dt
            speedY += ctrlY * dt
            motorX.on(speedX * 0.48)
            motorY.on((speedY + speedX * ratio) * 0.48)

        elif cmd == 'SHUTDOWN':
            motor1.off()
            motor2.off()
            break
except Exception as e:
    error_msg = "ERROR: " + str(e) + "\n"
    conn.sendall(error_msg.encode())
