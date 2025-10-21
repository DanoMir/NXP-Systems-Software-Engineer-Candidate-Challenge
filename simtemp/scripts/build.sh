#!/bin/bash
#...\NXP_Systems_Software_Engineer_Candidate_Challenge\simtemp\scripts\build.sh

#---DECLARATION OF VARIABLES----
KERNEL_VERSION=$(uname -r)
KERNEL_DIR=/lib/modules/${KERNEL_VERSION}/build
MODULE_NAME="nxp_simtemp"
KERNEL_SCR_PATH="../kernel"
USER_SCR_PATH="../user/cli"

#Error Handling: Prints the message and returns 1

fail()
{
    echo "--- ERROR: $1 ---" >&2
    exit 1
}

# --- 1 VERIFICATION AND SETUP ENVIRONMENT
echo "----- 1. Checking environment and Kernel Headers ----"

if [! -d "$KERNEL_DIR"]; then
    fail "Kernel Headers not found for $KERNEL_VERSION. Please install with: 'sudo apt install linux-headers-$KERNEL_VERSION'"
fi

# --- 2 Compilation of Kernel Modules in C files ---
echo "-----2. Compilation of Kernel Modules: ${MODULE_NAME}.ko ----"

# Compilation 
make -C "$KERNEL_DIR" M="$(pwd)/$KERNEL_SRC_PATH" modules CFLAGS_MODULE="-Werror" || fail "Compilation Failed of Kernel Module"

# --- 3 Configuration of Kernel environment ----
echo "-----3. Configuring Python Virtual Environment"
cd "$USER_SRC_PATH" || fail "User Directory could not be accessed."

#Creation of Virtual Environment if does not exist.

if [ ! -d "venv"]; then
    python3 -m venv venv || fail "Creation of Virtual Environment: Failed."
fi

#Activate the Virtual Environmentto install dependencies
source venv/bin/activate

#Install dependencies from requirements.txt
if [ -f "requirements.txt"]; then
    echo " Installing Python Dependencies from requirements.txt"
    pip install -r requirements.txt || fail "Installation of Python Dependencies Failed"
fi

#Deactivate venv for the bash does not remain in the environment
Deactivate

#Return to directory scripts/
cd - > /dev/null

echo "----- SUCCESS: Compilation and Setup Completed"