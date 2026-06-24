import serial
import statistics
import time
import matplotlib.pyplot as plt
import numpy as np
import math

#experimentally determined offsets  

X_OFFSET = 1.94
Y_OFFSET = -43.01
Z_OFFSET = -36.79

with serial.Serial("COM5", 115200, timeout= 2) as ser:

    num = ser.write(b'U')
    print(num)
    ser.flush()
    start_str = ser.read(5)
    print(start_str)
    number_of_samples = 10
    datasize = number_of_samples.to_bytes(2, byteorder="big")
    num = ser.write(datasize)
    print(num)

    samples = []
    for i in range (0, number_of_samples):
        line = ser.readline().decode()
        if(line == "OK\n"):
            data = ser.read(96)
            for i in range(0, len(data), 6):
                x = int.from_bytes(data[i:i+2], "little", signed=True)
                y = int.from_bytes(data[i+2:i+4], "little", signed=True)
                z = int.from_bytes(data[i+4:i+6], "little", signed=True)
                print(f"{x}, {y}, {z}")
                samples.append((x,y,z))

        else:
            print(f"ERROR OCCURED: {line}")


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

    SCALE = 0.0039

    g_total = [
        math.sqrt(x*x + y*y + z*z)
        for x, y, z in samples
    ]

    print(SCALE * sum(g_total) / len(g_total))

    #xpoints = np.array(1, number_of_samples)

    x = [s[0] * SCALE for s in samples]
    y = [s[1] * SCALE for s in samples]
    z = [s[2] * SCALE for s in samples]

    plt.plot(x)
    plt.plot(y)
    plt.plot(z)
    plt.xlabel("Sample")
    plt.ylabel("ADXL value in g")
    plt.title("ADXL345 measurements")
    plt.legend()
    plt.grid()
    plt.show()

    plt.hist(z, bins=20)
    plt.grid()
    plt.show()