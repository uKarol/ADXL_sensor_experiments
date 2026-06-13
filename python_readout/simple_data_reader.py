import serial
import statistics
import time

with serial.Serial("COM5", 115200, timeout= 2) as ser:

    num = ser.write(b'U')
    print(num)
    ser.flush()
    start_str = ser.read(5)
    print(start_str)
    number_of_samples = 500
    datasize = number_of_samples.to_bytes(2, byteorder="big")
    num = ser.write(datasize)
    print(num)

    samples = []
    for i in range (0, number_of_samples):
        line = ser.readline().decode().strip()
        segments = line.split(',')
        try:
            if(segments[0] == 'OK'):
                print(f'x: {int(segments[1])}, y: {int(segments[2])}, z: {int(segments[3])}' )
                x = int(segments[1])
                y = int(segments[2])
                z = int(segments[3])
                samples.append((x,y,z))

            else:
                print(f"ERROR OCCURED: {segments}")
        except (ValueError, IndexError):
            print("BAD ENTRY DATA")

    avg_x = sum(s[0] for s in samples) / len(samples)
    avg_y = sum(s[1] for s in samples) / len(samples)
    avg_z = sum(s[2] for s in samples) / len(samples)

    std_x = statistics.stdev(s[0] for s in samples)
    std_y = statistics.stdev(s[1] for s in samples)
    std_z = statistics.stdev(s[2] for s in samples)

    min_x = min(s[0] for s in samples)
    min_y = min(s[1] for s in samples)
    min_z = min(s[2] for s in samples)

    max_x = max(s[0] for s in samples)
    max_y = max(s[1] for s in samples)
    max_z = max(s[2] for s in samples)

    print(f"X average: {avg_x}, std dev {std_x}, max {max_x}, min {min_x} ")
    print(f"Y average: {avg_y}, std dev {std_y}, max {max_y}, min {min_y} ")
    print(f"Z average: {avg_z}, std dev {std_z}, max {max_z}, min {min_z} ")