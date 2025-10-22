import os
import select
import struct
import sys
import time
import argparse
from datetime import datetime, timezone 

# --- Contract Configuration ---

# Data Access Point: Path to file of Character Device created by Driver
DEVICE_PATH = "/dev/simtemp"

# Configuration access point: Directory where the Control Files are set.
SYSFS_BASE_PATH = "/sys/devices/platform/nxp_simtemp" 

#Size of Register simtemp_sample (bytes) for os.read()
SAMPLE_SIZE = 16  

# Unpack Binary Format to correct interpretation from.
STRUCT_FORMAT = '<Qil' 
# < - Little Endian
# Q - u64 timestamp
# i - s32 temp
# l - u32 flags

TIMEOUT = 5000

#
TEMP_DIVISOR = 1000.0

# Alert Bit Value for AND operator (&)
FLAG_THRESHOLD_CROSSED = 0x02 

#
FLAG_NEW_SAMPLE = 0x01

# --- Auxiliar Functions Definitions ---

# Configuration Writing: Control Interface
# Send configuration comands to Driver Kernel
# From run_demo.sh
# attribute: 
# value: 
def write_sysfs(attribute, value):
    """Write one value in sysfs attribute Sysfs. Requires root permissions."""
    #Construct the path
    path = os.path.join(SYSFS_BASE_PATH, attribute)
    try:
        with open(path, 'w') as f:
            
            #print(f"Writing in {path}...")
            #Internal syscall 'write()' to sysfs and 'sampling_ms_store()' and 'threshold_mC_store'
            f.write(str(value) + '\n')
    except Exception as e:
        # if not used 'sudo'
        # Standard error to shell run_demo.sh 
        print(f"Error writing in {path}: {e}", file=sys.stderr)
        #for stop the test sequence and report.
        sys.exit(1)



# fd: file descriptor from /dev/simtemp
# fd---struct file *file---- file->private_data------struct nxp_simtemp_dev
def read_and_print_sample(fd):
    """Read and process an Binary Register from kernel."""
    try:
        # Call to system syscall 'read()' to fd and size of data from Driver
        #fd obtained from open()
        data = os.read(fd, SAMPLE_SIZE) 
        
        # Verifies lenght of data
        if len(data) != SAMPLE_SIZE:
            return False
        
        # Data Validation
        # Unpacking data
        timestamp_ns, temp_mC, flags = struct.unpack(STRUCT_FORMAT, data)
        temp_C = temp_mC / TEMP_DIVISOR
        timestamp_sec = timestamp_ns / 1_000_000_000.0  
        
        # ISO 8601 Time Zone 'Z'
        date_time = datetime.fromtimestamp(timestamp_sec, tz=timezone.utc).isoformat().replace('+00:00', 'Z')

        #Isolation of Alert Bit from flags
        alert_status = 1 if (flags & FLAG_THRESHOLD_CROSSED) else 0

        print(f"{date_time} temp={temp_C:.1f}C alert={alert_status} | KERNEL FLAGS: {flags}")
        return True
    except Exception:
        return False
    
# --- Operation Mode 1 :Continuous Monitoring (Asynchronous Reading) ---

def cli_monitor_mode(args):
    """Continuous Monitoring using select.poll to wait Kernel Events."""
    print(f"Starting asynchronous monitoring in {DEVICE_PATH}. Press Ctrl+C to stop.")

    try:
        # Open with O_NONBLOCK.  Critical for poll to work.
        # Obtains fd.
        # os.O_RDONLY: Flag for only read. 
        # os.O_NONBLOCK: Critical Flag for read() calls. No bloaking when No Data, 
        # ...in this place must be fail with BlockingIOError instead 
        fd = os.open(DEVICE_PATH, os.O_RDONLY | os.O_NONBLOCK)
    except OSError:
        # Standard error to shell run_demo.sh 
        print(f"Error: File could not be opened {DEVICE_PATH}.", file=sys.stderr)
        sys.exit(1)

    # Waiting Event
    # Creation of objects Poll and Epoll.
    poller = select.poll()
    # Ask to SO if an Event exist.
    # select.POLLIN: fd is ready for a normal reading (new sampla available)
    # select.POLLPRI: fd is ready for a  priority reading (threshold was crossed)
    poller.register(fd, select.POLLIN | select.POLLPRI) 

    # Main Asynchronous Wait Loop 
    try:
        while True:
            # CPU Waits (sleep) until wake_up_interruptible() from hrtimer_restart 
            # wait 5000ms. Timeout for security to avoid sleeping forever
            #events from Wait Queue (wq)
            events = poller.poll(5000)
            
            # If Kernel Wakes-up and wake_up_interruptible() is called.
            # POLLIN or POLLPRI
            if events:
                
                # --- DEBUG -------------
                #print("-" * 50, file=sys.stderr)
                #print(f"DEBUG: Kernel reports {len(events)} events.", file=sys.stderr)
                #print(f"DEBUG: List of raw events : {events}", file=sys.stderr)
                #print("-" * 50, file=sys.stderr)
                # ------------------------------
                
                # Unpackin 'events' from poll()/epoll(), 'events' contains:
                # event_fd: Identifies the file (/dev/simtemp) 
                # mask: Indicates the event (1=DATA, 3=DATA+ALERT), from nxp_simtemp_poll() and returns POLLIN and POLLPRI flags
                for (event_fd, mask) in events:
                    #Verifies iof the event is from (/dev/simtemp)
                    if event_fd == fd:
                        # Batch Reading
                        # Reading dates repeatedly through infinite loop
                        while True:
                            
                            try:
                                #if reads 0bytes through implementation of os.read()
                                if not read_and_print_sample(fd):
                                    break 
                            
                            # Ring Buffer is empty when os.read(O_NONBLOCK)
                            # Breaks the loop and returns to sleep in poller.poll()
                            # -EAGAIN in C (Would Block Error)
                            except BlockingIOError:
                                break 
                            
                            #unspecified error
                            except Exception:
                                break 
                            #finally implementation
                            #finally:
                                #os.close(fd)
                                #poller.unregistered(fd)
        #if False: returns to loop                    
    # When press Ctrl+C        
    except KeyboardInterrupt:
        print("\nMonitoring stopped by User.")
    finally:
        # Closes the File Descriptor and unregister the poller.
        os.close(fd)
        poller.unregister(fd)


# --- Operation Mode 2: Threshold Test  ---

def cli_test_mode(args):
    """Executes a forced test of umbral and reports SUCESSFUL/FAILED."""
    TEST_THRESHOLD = 4000  # Low Umbral forced (4.0C)
    TIMEOUT_MS = 500       # 500ms wait

    #fd from (/dev/simtemp) is open in Non Blocking Mode or Read Mode for poll()  
    try:
        fd = os.open(DEVICE_PATH, os.O_RDONLY | os.O_NONBLOCK)
    except OSError:
        sys.exit(1)

    # 1. Configuration of Sysfs Test
    print(f"--- STARTING ALERT TEST (Threshold={TEST_THRESHOLD/1000.0}C) ---")
    write_sysfs("threshold_mC", TEST_THRESHOLD)
    write_sysfs("sampling_ms", 100) # Asegurar una frecuencia conocida

    # Waiting Event
    # Creation of objects Poll and Epoll.

    poller = select.poll()
    # In this case only waiting the Alert Event (POLLPRI) instead only one input
    poller.register(fd, select.POLLPRI) 

    # Waiting event.
    # CPU Waits (sleep) until wake_up_interruptible() from hrtimer_restart 
    # wait 5000ms. Timeout for security to avoid sleeping forever
    # events from Wait Queue (wq)
    events = poller.poll(TIMEOUT_MS) 

    if events:
        for (event_fd, mask) in events:
            #Conditional if event belongs to fd and if select.POLLPRI flag exist
            if event_fd == fd and (mask & select.POLLPRI):
                # Successful: Event is detected
                print("--- SUCCESS: POLLPRI Event (Threshold Alert) detected.")
                
                # Optional: Cleaning the flag in Kernel for the next test.
                # if os.path.exists(os.path.join(SYSFS_BASE_PATH, "clear_alert")):
                #    write_sysfs("clear_alert", 1) 
                
                # Closes the File Descriptor and unregister the poller.
                os.close(fd)
                sys.exit(0) # Successful Code
    
    # if no existence of Threshold Alert within TIMEOUT_MS 
    print(f">> FAIL: Umbral Alert not detected within the time limit.")
    
    # Closes the File Descriptor and unregister the poller.
    os.close(fd)
    sys.exit(1) # Fail Code


# --- Main Entry Point ---

if __name__ == "__main__":
    if not os.path.exists(DEVICE_PATH):
        print(f"Error: {DEVICE_PATH} does not exist. Please load the module first.", file=sys.stderr)
        sys.exit(1)

    parser = argparse.ArgumentParser(description="NXP Virtual Sensor CLI Tool.")
    parser.add_argument('--sampling-ms', type=int, help='Set sampling period in milliseconds via sysfs.')
    parser.add_argument('--threshold-mC', type=int, help='Set alert threshold in milli-Celsius via sysfs.')
    parser.add_argument('--test', action='store_true', help='Run threshold test mode and exit with success/failure code.')

    args = parser.parse_args()

    if args.test:
        cli_test_mode(args)
    else:
        # Aplicar configuraciones antes de iniciar el monitoreo continuo
        if args.sampling_ms:
            write_sysfs("sampling_ms", args.sampling_ms)
        if args.threshold_mC:
            write_sysfs("threshold_mC", args.threshold_mC)
        
        # Iniciar monitoreo
        cli_monitor_mode(args)
        
                                                         