#!/bin/bash
# scripts/build.sh: Automates the compilation of Kernel Drive and the setup of Python.

# --- DECLARATION OF VARIABLES ----
KERNEL_VERSION=$(uname -r)
KERNEL_DIR=/lib/modules/${KERNEL_VERSION}/build
MODULE_NAME="nxp_simtemp"
KERNEL_SRC_PATH="../kernel"
USER_SRC_PATH="../user/cli"
REQUIREMENTS_FILE="requirements.txt"

# --- ABSOLUTE PATH to directory 'kernel' (executed from simtemp/scripts)
KERNEL_MODULE_PATH="$(pwd)/$KERNEL_SRC_PATH" 

# Error Handling: Prints the message and returns 1
fail()
{
    echo "--- ERROR: $1 ---" >&2
    exit 1
}

# --- 1. VERIFICATION AND SETUP ENVIRONMENT ---
echo "----- 1. Checking environment and Kernel Headers -----"

if [ ! -d "$KERNEL_DIR" ]; then
    fail "Kernel Headers not found for $KERNEL_VERSION. Please install with: 'sudo apt install linux-headers-$KERNEL_VERSION'"
fi

# --- 2. COMPILATION AND CLEANING KERNEL (C) ---
echo "----- 2. Compiling Kernel Module: ${MODULE_NAME}.ko ----"

# Cleaning: Ensures that the Make clean uses the absolute path to Kernel directory
echo "------- Executing make clean... --------"
make -C "$KERNEL_DIR" M="$KERNEL_MODULE_PATH" clean || fail "Kernel Module Clean Failed"

# Compilation: Construct the module
make -C "$KERNEL_DIR" M="$KERNEL_MODULE_PATH" modules CFLAGS_MODULE="-Werror" || fail "Compilation Failed of Kernel Module"

# --- 3. CONFIGURATION OF PYTHON ENVIRONMENT (User Space) ---
echo "----- 3. Configuring Python Virtual Environment (User Space) -----"

VENV_NAME="nxp_env"
LOCAL_VENV_PATH="/tmp/$VENV_NAME"

# ABSOLUTE Path to directory of the origin dependencies
CLI_SOURCE_PATH="$(pwd)/$USER_SRC_PATH"
REQUIREMENTS_FILE="$CLI_SOURCE_PATH/requirements.txt"

#cd "$USER_SRC_PATH" || fail "User Directory could not be accessed."

#-----------Option without Virtual Machine--------------

# Creation (using 'sudo' for permissions in shared folders)
# if [ ! -d "venv" ]; then
#     # Usamos sudo solo para crear la estructura venv si no existe
#     #sudo python3 -m venv venv  || fail "Creation of Virtual Environment: Failed."
#     sudo python3 -m venv venv --copies || fail "Creation of Virtual Environment: Failed."
# fi


#-----------Option with Virtual Machine----------------------
# 2. Creation of VENV in a secure directory (/tmp)
if [ ! -d "$LOCAL_VENV_PATH" ]; then
    echo " ------- Creating venv in $LOCAL_VENV_PATH (Outside the Shared Folder of Virtual Machine)...  ----------"
    # IMPORTANT: Not using sudo, since /tmp is a local directory
    python3 -m venv "$LOCAL_VENV_PATH" || fail "Creation of Virtual Environment: Failed."
fi

# Installation of dependencies (using the binary of Python inside of venv)

#-----------Option without Virtual Machine--------------
#PYTHON_VENV_EXEC="./venv/bin/python3"
#PYTHON_VENV_PIP="./venv/bin/pip" # Defines the path to binary pip

#-----------Option with Virtual Machine----------------------
PYTHON_VENV_PIP="$LOCAL_VENV_PATH/bin/pip"

if [ -f "$REQUIREMENTS_FILE" ]; then
    echo "-------- Installing Python Dependencies from requirements.txt --------"
    # Calling to pip of virtual environment (through of python binary)
    # and using sudo because venv was created by root.
    #sudo "$PYTHON_VENV_EXEC" -m pip install -r "$REQUIREMENTS_FILE" || fail "Installation of Python Dependencies Failed"
    #sudo bash -c "source venv/bin/activate && pip install -r requirements.txt" || fail "Installation of Python Dependencies Failed"
    sudo "$PYTHON_VENV_PIP" install -r "$REQUIREMENTS_FILE" || fail "Installation of Python Dependencies Failed"
fi

# 3.3 Return to path scripts/

cd - > /dev/null

echo "----- SUCCESS: Compilation and Setup Completed -------------"