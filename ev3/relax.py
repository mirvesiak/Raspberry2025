from ev3dev2.motor import Motor, OUTPUT_C
from time import sleep
m = Motor(OUTPUT_C)
m.duty_cycle_sp=35
m.run_direct()
sleep(2)
m.stop_action = 'coast'
m.stop()