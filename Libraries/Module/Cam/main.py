from machine import UART
import sensor
import time
import pyb
import math
import struct

def init():
  sensor.reset()
  sensor.set_pixformat(sensor.RGB565)
  sensor.set_framesize(sensor.QQVGA)
  sensor.skip_frames(time=2000)
  sensor.set_auto_gain(False)
  sensor.set_auto_whitebal(False)

R_led = pyb.LED(1)  # Red LED
G_led = pyb.LED(2)  # Green LED
B_led = pyb.LED(3)  # Blue LED

init()
clock = time.clock()
uart = UART(3, 9600)

# ball = (18, 50, 12, 54, 10, 56)
blue_goal = (21, 51, -128, 127, -79, -42)

data = {}

bottom_y = sensor.height() - 1
middle_x = sensor.width() // 2
orign = (middle_x, bottom_y)

# G_led.on()  # turn on green LED to indicate camera is ready

HEADER = b'\xFF'
FOOTER = b'\xFE'
def send_data(data):
  dataBytes = bytes(data)
  packet = HEADER + dataBytes + FOOTER
  uart.write(packet)

while True:
  clock.tick()

  img = sensor.snapshot()

  blobs = img.find_blobs([blue_goal])

  if blobs:
    # use the largest blobs
    largest_blob = max(blobs, key=lambda b: b.pixels())
    img.draw_rectangle(largest_blob.rect())
    img.draw_cross(largest_blob.cx(), largest_blob.cy())

    # store blob data in a dictionary
    data['x'] = largest_blob.cx()
    data['y'] = largest_blob.cy()

    ### Below data is not needed for angle calculation
    # data['w'] = largest_blob.w()
    # data['h'] = largest_blob.h()
    # data['pixels'] = largest_blob.pixels()
    # data['area'] = largest_blob.area()

    # find the angle from the origin to the blob center
    dx = data['x'] - middle_x
    dy = bottom_y - data['y']
    angle = math.atan2(dx, dy) * 180 / math.pi
    data['angle'] = angle * -1  # change sign

    # draw line from origin to blob center
    img.draw_cross(middle_x, bottom_y)
    img.draw_line(middle_x, bottom_y, data['x'], data['y'])

    B_led.on()

  else:
    # if no blobs found, set angle to 999 to indicate no goal found
    data['angle'] = 999

    B_led.off()

  # print data to console for debugging
  print(str(data) + '\n')

  # send data (only angle) to the stm32
  angle = struct.pack('<f', data['angle'])   # '<f' = little-endian float
  send_data(angle)
