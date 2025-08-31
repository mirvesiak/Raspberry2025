#!/usr/bin/env python3
from ev3dev2.motor import Motor, OUTPUT_A, OUTPUT_B, SpeedPercent
from ev3dev2.button import Button
from time import sleep

# Initialize motors
motorA = Motor(OUTPUT_A)  # Left/Right
motorB = Motor(OUTPUT_B)  # Up/Down

# Initialize buttons
btn = Button()

# Speed and duration settings
speed = 30
run_time = 0.1  # in seconds

print("Ready for button input...")

while True:
    speedA = 0
    speedB = 0
    if btn.left:
        speedA = speed
    elif btn.right:
        speedA = -speed
    elif btn.up:
        speedB = speed
    elif btn.down:
        speedB = -speed
    elif btn.enter:
        break
    motorA.on(speedA)
    motorB.on(speedB + speedA * 5/7)
    sleep(0.05)  # small delay to avoid overwhelming the CPU
    motorA.on(0)
    motorB.on(0)
