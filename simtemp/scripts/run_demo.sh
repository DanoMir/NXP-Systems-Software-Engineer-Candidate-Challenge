#!/bin/bash
# Internal Name of Module, contained in .driver.name from 'platform_driver'
MODULE_NAME="nxp_simtemp"
# Path to the file nxp_simtemp.ko by default
MODULE_PATH="../kernel/${MODULE_NAME}.ko"
# Syfs control 
# Obtains sysfs files: sampling_ms and threshold_mC
SYSFS_DEVICE_DIR="/sys/devices/platform/$MODULE_NAME"
# Char Device Files created by Platform Driver
DEVICE_FILE="/dev/simtemp"
# Defines the path for sysfs.
SYSFS_PATH="/sys/bus/platform/devices/simtemp0" 

run_threshold_test() 
{
    echo "--- 3. Executing CLI Alert Test ---"
    # Python Script in --test execute the test and return 0 or 1
    python3 ../user/cli/main.py --test
    
    # Capture the exit code (0 = Successful, 1 = Failed)
    return $?
}


# --- START ---
echo "--- 1. Loading Driver Kernel ---"
#Load the Binary Module to Kernel 
#From Kernel Space calls to function nxp_simtemp_probe().
sudo insmod ../kernel/nxp_simtemp.ko || { echo "ERROR: Failed insmod."; exit 1; }
sleep 1
#sudo chmod 666 "$SYSFS_PATH/sampling_ms"
#sudo chmod 666 "$SYSFS_PATH/threshold_mC"
#sudo chmod 666 /dev/simtemp
#sleep 1
echo "--- 2.  Sysfs Permissions  ---"
# Existence Verification of Directory (to prevent it from failing chmod)
if [ -d "$SYSFS_DEVICE_DIR" ]; then
    # Within sysfs files:
    # Writing permits granted (w) to user
    # Reading permits granted (R) to user
    sudo chmod 666 "$SYSFS_DEVICE_DIR/sampling_ms"
    sudo chmod 666 "$SYSFS_DEVICE_DIR/threshold_mC"
    sudo chmod 666 "$SYSFS_DEVICE_DIR/stats"
    #Permits Granted for User to open and read '/dev/simtemp' 
    sudo chmod 666 "$DEVICE_FILE"
    echo "--- sysfs Permissions updated.---"
else
    echo "WARNING: sysfs Directory $SYSFS_DEVICE_DIR Not found. Continue..."
fi
sleep 2

# --- Testing ---
run_threshold_test
TEST_RESULT=$? # Saves output code (0 o 1)

if [ $TEST_RESULT -eq 0 ]; then
    echo "--- SUCCESS: Alert (POLLPRI) works correctly. ---"
else
    echo "--- FAILED: Alert (POLLPRI) failed the test. ---"
fi

# --- Cleaning 1 ---
echo "--- 4. Unloading Driver Kernel: $MODULE_NAME ---"
# rmmod is executed regardless if the test failed or not
sudo rmmod "$MODULE_NAME" || { echo "WARNING: rmmod failed. Revisa dmesg."; exit 1; }


exit $TEST_RESULT



echo "--- 2. Monitoring and Configuration ---"
# Run Python Script , with rate configuration to 200ms
# From here verify calls: read_telemetry_epoll and write_sysfs.
python3 ../user/cli/main.py --sampling-ms 200
sleep 3
#Module Unloaded
#From Kernel Space calls to function nxp_simtemp_remove(). Stops hrtimer, deletes sysfs and Char Device.
echo "--- 3. Unloading Driver Kernel: $MODULE_NAME ---"
sudo rmmod "$MODULE_NAME" || { echo "WARNING: rmmod failed."; exit 1; }
sleep 4
echo "End of Driver Implementation "
#Stops with Ctrl+C