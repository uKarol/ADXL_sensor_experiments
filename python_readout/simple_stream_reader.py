import serial 

with serial.Serial("COM5", 115200) as ser:

    data = ser.read(96)

    for i in range(0, len(data), 6):
        x = int.from_bytes(data[i:i+2], "little", signed=True)
        y = int.from_bytes(data[i+2:i+4], "little", signed=True)
        z = int.from_bytes(data[i+4:i+6], "little", signed=True)

        print(x, y, z)