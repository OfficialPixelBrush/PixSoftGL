import serial

ser = serial.Serial("/dev/ttyACM0", 115200, timeout=1)
ser.write(b"\x06\x33\x0D")

data = ser.read(4)
value = int.from_bytes(data, byteorder="little", signed=True)
print(value)