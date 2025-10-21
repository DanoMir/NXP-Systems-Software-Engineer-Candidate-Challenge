# NXP-Systems-Software-Engineer-Candidate-Challenge
This repository contains the source code for the "Virtual Sensor + Alert Path".

# Welcome to NXP Systems Software Engineer Candidate Challenge!

## About
# NXP Virtual Sensor Platform Driver (nxp_simtemp)
This driver is implemented as an **out-of-tree Platform Driver (Kernel Space)** to simulate a sensor, ensure data integrity, and monitor temperature values against a configurable threshold.

## Project locations & Systems

* Platform Driver (Kernel Space)
..\kernel\nxp_simtemp.c

* CLI app (User Space)
..\user\cli\main.py

* Device Tree Snipset (DT)
..\kernel\dts\nxp-simtemp.dtsi

* Toolchain Automation
..\scripts\build.sh


### Source Control
* GIT Server with [NXP Systems Software Engineer Candidate Challenge](https://github.com/DanoMir/NXP-Systems-Software-Engineer-Candidate-Challenge.git)

### Video Demo
*

### Operative System
Ubuntu LTS 24.04 (64-bit)
6.14.0-33-generic

-Check Ubuntu Version.

```bash
    uname -r

if is different to 6.14.0-33-generic

Please change to Ubuntu version for correct functioning of build.sh

* Prerequisites in Linux Environment

After having Ubuntu Ubuntu LTS 24.04 (64-bit) 6.14.0-33-generic
    ```bash
    - Base Development Tools
    sudo apt update
    sudo apt install gcc make build-essential

    - Kernel Headers
    sudo apt install linux-headers-$(uname -r)

    - Tools installation for lint.sh
    sudo apt install clang-format -y
    sudo apt install kernel-manual -y
    sudo apt install linux-tools-common -y


### Python Version
Python 3.12.3

-If not Python installed, please install
```bash
    sudo apt update
    sudo apt install python3

-Preferably make sure that the required version is installed ALWAYS 3.12 version.
```bash
    sudo apt update
    sudo apt install python3 python3.12-venv

-ONLY if is required the specific version to run.
*Navigates to path directory inside the project downloaded form the repository.
```bash
    cd ..\NXP_Systems_Software_Engineer_Candidate_Challenge\simtemp\user\cli
    python3.12 -m venv venv_312
    source venv_312/bin/activate

-ONLY if Linux does not find the specific version.
*Install pyenv
```bash
    curl https://pyenv.run | bash
    (Follow the instructions of installation)

*Install the specific version.
```bash
    pyenv local 3.12.3

*Now 'python3' in this directory will use 3.12.3 version
```bash
    python3 -m venv venv 
    source venv/bin/activate



### Notes on using a Vitual Machine

This project was developed and validated in Virtual Machine Ubuntu 24.04 LTS (x86_64).
Kernel 6.14.0-33-generic

*Review your version with
```bash
    sudo (uname -r)

### Some issues resolved
* in run_demo.sh
SYSFS_DEVICE_DIR="/sys/devices/platform/$MODULE_NAME"
MODULE_NAME nxp_simtemp

if you have more driver installed please check which was the last module installed with

```bash
    lsmod

It will display a list of active modules

(example)

```bash
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
```bash
    sudo $(uname -r)

Kernel Headers Installation with the correct version.

```bash
    sudo apt update
    sudo apt install linux-headers-6.14.0-33-generic

if the version is not found. Install the recent version.

```bash
    sudo uname -r

* Lack of User Space dependencies

The environment could not have the specific libraries in Python

    run_demo.py fails with 'ModuleNotFoundError'

```bash 
    cd ...\NXP_Systems_Software_Engineer_Candidate_Challenge\simtemp\user\cli
    python3 -m venv venv

Activate the environment:

```bash
    source venv/bin/activate

Install dependencies:

```bash
    pip install -r requirements.txt

After using the environment:

```bash
    deactivate

* EOL format conflict in bash during run .sh files:

When executing either 'bash run_demo.sh' or 'bash build.sh' displays type errors such as:

run_demo.sh: line X: $'\r': command not found (example)

This bash scripts .sh were created in Windows (CRLF) and were transfered to Linux environment.

It is neccesary convert the files .sh to Unix Format (LF)

```bash
    sudo apt install dos2unix
    dos2unix scripts/run_demo.sh
    dos2unix scripts/build.sh

### Execution and Validation (Core Acceptance Criteria)

3. Execution and Validation (Core Acceptance Criteria)
The run_demo.sh script automates the full test sequence (Load, Permissions, Test, Unload).

A. Run Automated Threshold Test (--test mode)
This Mode verifies the critical Alert Path (POLLPRI) functionality and report success or failure.
```bash
    chmod +x run_demo.sh
    # Execute the full test sequence: insmod -> config -> python --test -> rmmod
    ./run_demo.sh 
    Expected Output: --- ÉXITO: La Ruta de Alerta (POLLPRI) funciona correctamente. --- Exit Code: echo $? must return 0.

B. Run Continuous Monitoring Demo
To observe live data and dynamic configuration:
```bash
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