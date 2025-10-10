# main.py
import struct
import time
import os
from datetime import datetime

# Kernel Structure
#

#Defines the exact binary format you expect to receive from Kernel
#Matching with 'simtemp_sample' struct
# = ensures the standard data alineation
# Q u64 timestamp in nanoseconds
# i s32 temperature in mili-grades.
# I u32 flags
SAMPLE_FORMAT = '=QiI'

#Total size for structure (16 bytes) for Character Device.
SAMPLE_SIZE = struct.calcsize(SAMPLE_FORMAT)

#Exact path of device node that Driver creates in System
DEVICE_PATH = '/dev/simtemp'
TEMP_DIVISOR = 1000.0           #Converts m°C to °C

def read_telemetry():
    print(f"------ NXP SimTemp CLI Reader -----")
    print(f"Waiting for data on {DEVICE_PATH}. Press Ctrl+C to Stop")
    
    try:
        # Opening the interface, calls to nxp_simtemp_open in Driver
        # Open device in binary mode (rb) binary reading byte to byte
        with open(DEVICE_PATH, 'rb') as f:
            while True:   
                # Reads the exact size of binary sample.
                # Calls to function nxp_simtemp_read.
                # Waits and expect 16 bytes
                # Blocking Reading
                data = f.read(SAMPLE_SIZE)
                
                # Verification.
                # Confirmes that a complete package of Kernel are received.
                if len(data) == SAMPLE_SIZE:
                    #Bynary Data are unpacked
                    timestamp_ns, temp_mC, flags = struct.unpack(SAMPLE_FORMAT, data)
                    
                    #Timestamp is converted to format
                    #Remember is nanoseconds
                    timestamp_s = timestamp_ns / 1_000_000_000.0
                    dt_object = datetime.fromtimestamp(timestamp_s).strftime('%Y-%m-%dT%H:%M:%S.%f')[:-3] + 'Z'
                    
                    #Determines Alerts
                    alert = "YES" if (flags & 0x02) else "NO"
                    
                    #Prints results in format
                    print(f"{dt_object} temp={temp_mC / TEMP_DIVISOR:.1f}C alert={alert} | Flags: {flags}")
                 
                elif len(data) > 0:
                    print(f"Warning: Received partial data ({len(data)} bytes). Discarding.")
                    
    except FileNotFoundError:
        print(f"ERROR: Device file not found at {DEVICE_PATH}. Is the kernel module loaded?")  
    except PermissionError:
        print(f"ERROR: Permission denied. Try running with 'sudo'.")
    except struct.error as e:
        print(f"ERROR: Data unpacking failed: {e}. Possible corruption")
    except KeyboardInterrupt:
        print("\n Reader stopped by user.")
        os._exit(0) #Clean output
    except Exception as e:
        print(f"An unexpected error ocurred: {e}")
        
if __name__ == "__main__":
    #Quick Check
    #Assume that the module is loades before to execute.
    
    if not os.path.exists(DEVICE_PATH):
        print(f"Error: {DEVICED_PATH} does not exists. Please load the module first.")
    else:
        read_telemetry()     