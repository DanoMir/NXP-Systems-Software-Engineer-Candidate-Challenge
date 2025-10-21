# NXP-Systems-Software-Engineer-Candidate-Challenge
This repository contains the source code for the "Virtual Sensor + Alert Path".

# Welcome to NXP Systems Software Engineer Candidate Challenge!

## About
# NXP Virtual Sensor Platform Driver (nxp_simtemp)
This Driver is created and implemented entirely in Linux Kernel Space as an out-of-free platform driver. Its primary funcion is to simulate a temperature sensor, generate samples using a timer and ensuring data integrity.

## Project locations & Systems

* Platform Driver (Kernel Space)
..\NXP_Systems_Software_Engineer_Candidate_Challenge\simtemp\kernel\nxp_simtemp.c

* CLI app (User Space)
..\NXP_Systems_Software_Engineer_Candidate_Challenge\simtemp\user\cli\main.py

* Device Tree Snipset (DT)
..\NXP_Systems_Software_Engineer_Candidate_Challenge\simtemp\kernel\dts\nxp-simtemp.dtsi

* Toolchain Automation
..\NXP_Systems_Software_Engineer_Candidate_Challenge\simtemp\scripts\build.sh


### Source Control
* GIT Server with [NXP Systems Software Engineer Candidate Challenge](https://github.com/DanoMir/NXP-Systems-Software-Engineer-Candidate-Challenge.git)

### Video Demo
*

### Operative System
Ubuntu LTS 24.04 (64-bit)
6.14.0-33-generic

-Check Ubuntu Version.

'''Bash
    uname -r

if is different to 6.14.0-33-generic

Please change to Ubuntu version for correct functioning of build.sh

* Prerequisites in Linux Environment

After having Ubuntu Ubuntu LTS 24.04 (64-bit) 6.14.0-33-generic
    '''Bash
    - Base Development Tools
    sudo apt update
    sudo apt install gcc make build-essential

    - Kernel Headers
    sudo apt install linux-headers-$(uname -r)


### Python Version
Python 3.12.3

-If not Python installed, please install
'''Bash
    sudo apt update
    sudo apt install python3

-Preferably make sure that the required version is installed.
'''Bash
    sudo apt update
    sudo apt install python3 python3-venv

-ONLY if is required the specific version to run.
*Navigates to path directory inside the project downloaded form the repository.
'''Bash
    cd ..\NXP_Systems_Software_Engineer_Candidate_Challenge\simtemp\user\cli
    python3.12 -m venv venv_312
    source venv_312/bin/activate

-ONLY if Linux does not find the specific version.
*Install pyenv
'''Bash
    curl https://pyenv.run | bash
    (Follow the instructions of installation)

*Install the specific version.
'''Bash
    pyenv local 3.12.3

*Now 'python3' in this directory will use 3.12.3 version
'''Bash
    python3 -m venv venv 
    source venv/bin/activate

### Notes on using a Vitual Machine

This project was developed and validated in Virtual Machine Ubuntu 24.04 LTS (x86_64).
Kernel 6.14.0-33-generic

*Review your version with
'''Bash
    sudo $(uname -r)

### Some issues resolved
* in run_demo.sh
SYSFS_DEVICE_DIR="/sys/devices/platform/$MODULE_NAME"
MODULE_NAME nxp_simtemp

if you have more driver installed please check which was the last module installed with

'''Bash
    lsmod

It will display a list of active modules

(example)

'''Bash
    Module          Size    Used By 
    nxp_simtemp0    ----    -------
    ...

Please change the module name in 
    run_demo.sh
    SYSFS_DEVICE_DIR="/sys/devices/platform/$MODULE_NAME"
    MODULE_NAME nxp_simtemp0

* in nxp_simtemp.h

if exist a compatibility error with Kernel Headers .h

Please check the Kernel version 6.14.0-33-generic

if you have a different version, install the correct Kernel Version.

Review your version with
'''Bash
    sudo $(uname -r)

Kernel Headers Installation with the correct version.

'''Bash
    sudo apt update
    sudo apt install linux-headers-6.14.0-33-generic

if the version is not found. Install the recent version.

'''Bash
    sudo uname -r

* Lack of User Space dependencies

The environment could not have the specific libraries in Python

    run_demo.py fails with 'ModuleNotFoundError'

'''Bash 
    cd ...\NXP_Systems_Software_Engineer_Candidate_Challenge\simtemp\user\cli
    python3 -m venv venv

Activate the environment:

'''Bash
    source venv/bin/activate

Install dependencies:

'''Bash
    pip install -r requirements.txt

After using the environment:

'''Bash
    deactivate

### Execution and Validation (Core Acceptance Criteria)

3. Execution and Validation (Core Acceptance Criteria)
The run_demo.sh script automates the full test sequence (Carga, Permisos, Prueba, Descarga).

A. Run Automated Threshold Test (--test mode)
This mode verifies the critical Alert Path (POLLPRI) functionality and reports success or failure.

Bash

cd simtemp/scripts
chmod +x run_demo.sh

# Execute the full test sequence: insmod -> config -> python --test -> rmmod
./run_demo.sh 
Expected Output: --- ÉXITO: La Ruta de Alerta (POLLPRI) funciona correctamente. --- Exit Code: echo $? must return 0.

B. Run Continuous Monitoring Demo
To observe live data and dynamic configuration:

Bash

cd simtemp/scripts
# Execute the full sequence, setting sampling to 500ms
python3 ../user/cli/main.py --sampling-ms 500
# Press Ctrl+C to stop, then run sudo rmmod nxp_simtemp to clean up.

### Build Servers
* Not implemented: 

### Issues & Review
* For V1.0, issues and review in process after the release.

### Project Folder Shares
* NXP_Systems_Software_Engineer_Candidate_Challenge

### Sharepoint
* 

### Information Sharing 

This project is submitted with the intention of demonstrating skill development.
* **License:** The Kernel Module is licensed under the **GPL(General Public License)**, in compliance with Linux Kernel requirements.
* **Code Access:** The full source and documentation are public and available via the link provided in '### Source Control' section. 
**Usage:** The design is open for review, feedback and further development.