import matplotlib.pyplot as plt
import csv

def load_log(filename):
    times = []
    setpoints = []
    positions = []
    pid_outputs = []
    
    with open(filename, 'r') as f:
        reader = csv.reader(f)
        next(reader)  # Skip header
        for row in reader:
            times.append(int(row[0]) / 1000.0)  # ms → s
            setpoints.append(float(row[1]))
            positions.append(float(row[2]))
            pid_outputs.append(float(row[3]))
    
    return times, setpoints, positions, pid_outputs

# Load both logs
file1 = "outA_log.csv"
file2 = "outB_log.csv"

t1, s1, p1, o1 = load_log(file1)
t2, s2, p2, o2 = load_log(file2)

# Plotting
plt.figure(figsize=(12, 8))

# 1️⃣ Position vs Time
plt.subplot(2, 1, 1)
plt.plot(t1, s1, '--', label="Setpoint A", color='blue')
plt.plot(t1, p1, label="Position A", color='skyblue')
plt.plot(t2, s2, '--', label="Setpoint B", color='green')
plt.plot(t2, p2, label="Position B", color='lightgreen')
plt.ylabel("Position (degrees)")
plt.title("Motor Position vs Time")
plt.legend()
plt.grid(True)

# 2️⃣ PID Output vs Time
plt.subplot(2, 1, 2)
plt.plot(t1, o1, label="PID Output A", color='blue')
plt.plot(t2, o2, label="PID Output B", color='green')
plt.xlabel("Time (s)")
plt.ylabel("PID Output")
plt.title("PID Output vs Time")
plt.grid(True)
plt.legend()

plt.tight_layout()
plt.show()
