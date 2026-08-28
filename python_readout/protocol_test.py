import serial


def receive_data(ser:serial.Serial):
    x = ser.read(1)
    if(x == b'\x55'):
        type = ser.read(1)
        length = int.from_bytes(ser.read(2), byteorder="big")
        print(length)
        data = ser.read(length)
        
        if(type == b'\x02'):
            ONE_SAMPLE_SIZE = 6
            sample_ctr = 0
            for i in range(0, len(data), ONE_SAMPLE_SIZE):
                x = int.from_bytes(data[i:i+2], "little", signed=True)
                y = int.from_bytes(data[i+2:i+4], "little", signed=True)
                z = int.from_bytes(data[i+4:i+6], "little", signed=True)
                print(f"sample {sample_ctr}:  {x}, {y}, {z}")
        else:
            print(type)
            print(data)


with serial.Serial("COM5", 115200) as ser:
   # num = ser.write(b'U')
    #msg = b'\x55\x00\x05\x00\x11\x22\x33\x44\x55'
    msg = b'\x55\x00\x01\x00\x00'
    num = ser.write(msg)
    print(num)
    for i in range(0,10):
        receive_data(ser)
    msg = b'\x55\x00\x01\x00\x01'
    num = ser.write(msg)