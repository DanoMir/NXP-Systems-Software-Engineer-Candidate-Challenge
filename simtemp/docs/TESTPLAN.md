1. Objectives and scopes
The objective of this Test Plan is validate the behavior and robustness of Platform Driver Module 'nxp_simtemp' and its User Space Interface (CLI). The scope covers three communication routes: Data Control and Asyncronous Events.

2. Test Environment

* Execution Platform: Ubuntu 24.04 LTS (or similar versions) for Virtual Machine in Oracle Virtual Box Administrator
* Test Tool: Aplication CLI in Python 'main.py' excecuted in automation script 'run_demo.sh'.

3. Test Cases (Acceptance Criteria Validation)

- **T1 — Load/Unload:** build → `insmod` → verify `/dev/simtemp` & sysfs → `rmmod` (no warnings).
- **T2 — Periodic Read:** set `sampling_ms=100`; verify ~10±1 samples/sec using timestamps.
- **T3 — Threshold Event:** lower threshold slightly below mean; ensure `poll` unblocks within 2–3 periods and flag is set.
- **T4 — Error Paths:** invalid sysfs writes → `-EINVAL`; very fast sampling (e.g., `1ms`) doesn’t wedge; `stats` still increments.
- **T5 — Concurrency:** run reader + config writer concurrently; no deadlocks; safe unload.
- **T6 — API Contract:** struct size/endianness documented; user app handles partial reads.

====================================================================================================================================================
TEST CASE: T1 — Load/Unload
====================================================================================================================================================

| TEST                               | VALIDATION PROCESS                | TEST SUCCESS CRITERIA              | MODULES TESTED                     |
|------------------------------------|-----------------------------------|------------------------------------|------------------------------------|
| Clean Load.                        | Run in bash:                      | This comand must be return         | nxp_simtemp_probe()                |
|                                    | 'sudo insmod nxp_simtemp.ko'      | 'Code 0' and 'dmesg' from          |    misc_register                   |
|                                    |                                   | Log should not show any            |                                    |
|                                    |                                   | warning.                           |                                    |
|------------------------------------|-----------------------------------|------------------------------------|------------------------------------|
| Existence of Interface             | Verify that '/dev/simtep' file    | After line execution of 'chmod' in | misc_register                      |
| '/dev/simtemp'                     | has been created.                 | 'run_demo.sh' means that the file  |                                    |
|                                    |                                   | exist.                             |                                    |
|                                    |                                   |                                    |                                    |
|------------------------------------|-----------------------------------|------------------------------------|------------------------------------|
| Existence of Interface 'sysfs'     | Verify that 'sampling_ms'         | Files exists and 'nxp_simtemp'     | syfs_create_group()                |
|                                    | 'threshold_mC' and 'stats'        | driver can be unloaded cleanly     | nxp_simtemp_remove()               |
|                                    | has been created in device        | (rmmod without errors).            |                                    |
|                                    | directory.                        |                                    |                                    |
|------------------------------------|-----------------------------------|------------------------------------|------------------------------------|


====================================================================================================================================================
TEST CASE: T2 — Periodic Read
====================================================================================================================================================

| TEST                               | VALIDATION PROCESS                | TEST SUCCESS CRITERIA              | MODULES TESTED                     |
|------------------------------------|-----------------------------------|------------------------------------|------------------------------------|
| Data Binary Integrity.             | Executes the continuous monitoring| The CLI application must be read   | nxp_simtemp_read()                 |
|                                    | (python3 main.py).                | exactly 16 bytes by sample         | struct.unpack                      |
|                                    |                                   | (data packed) and unpack this      |                                    |
|                                    |                                   | sample though (<Qil) register      |                                    |
|                                    |                                   | without errors.                    |                                    |
|------------------------------------|-----------------------------------|------------------------------------|------------------------------------|
| Visualization of Dynamic Data.     | Observe the data output in real   | The timestamp must be correct      | simtemp_timer_callback() through:  |
|                                    | time.                             | according to actual dates of the   |   ktime_get_real_ns                |
|                                    |                                   | Operative System and temperature   |   get_random_bytes                 |
|                                    |                                   | values must be variable.           |                                    |
|------------------------------------|-----------------------------------|------------------------------------|------------------------------------|
| Sampling Rate Stability.           | Configuration of 'sampling_ms'    | The Log of CLI must be show the    | hrtimer_forward_now()              |
|                                    | to 500ms (via sysfs).             | samples spaced at intervals of     |                                    |
|                                    |                                   | 500 ms showing a stable monitoring.|                                    |
|                                    |                                   |                                    |                                    |
|------------------------------------|-----------------------------------|------------------------------------|------------------------------------|


==============================================================================================================
TEST CASE: T3 — Threshold Event
==============================================================================================================
| TEST                               | VALIDATION PROCESS                | TEST SUCCESS CRITERIA              | MODULES TESTED                     |
|------------------------------------|-----------------------------------|------------------------------------|------------------------------------|
| Automated Testing.                 |                                   |                                    |                                    |
|                                    |                                   |                                    |                                    |
|                                    |                                   |                                    |                                    |
|                                    |                                   |                                    |                                    |
|------------------------------------|-----------------------------------|------------------------------------|------------------------------------|
|             |   |   |   |
|                                    |   |   |   |
|                                    |   |   |   |
|                                    |   |   |   |
|------------------------------------|---|---|---|
|            |   |   |   |
|                                    |   |   |   |
|                                    |   |   |   |
|                                    |   |   |   |
|------------------------------------|---|---|---|

==============================================================================================================
TEST CASE: T4 — Error Paths
==============================================================================================================
| TEST                               | VALIDATION PROCESS                | TEST SUCCESS CRITERIA              | MODULES TESTED                     |
|------------------------------------|-----------------------------------|------------------------------------|------------------------------------|
| Automated Testing.                 |                                   |                                    |                                    |
|                                    |                                   |                                    |                                    |
|                                    |                                   |                                    |                                    |
|                                    |                                   |                                    |                                    |
|------------------------------------|-----------------------------------|------------------------------------|------------------------------------|
|             |   |   |   |
|                                    |   |   |   |
|                                    |   |   |   |
|                                    |   |   |   |
|------------------------------------|---|---|---|
|            |   |   |   |
|                                    |   |   |   |
|                                    |   |   |   |
|                                    |   |   |   |
|------------------------------------|---|---|---|

==============================================================================================================
TEST CASE: T5 — Concurrency
==============================================================================================================
| TEST                               | VALIDATION PROCESS                | TEST SUCCESS CRITERIA              | MODULES TESTED                     |
|------------------------------------|-----------------------------------|------------------------------------|------------------------------------|
| Automated Testing.                 |                                   |                                    |                                    |
|                                    |                                   |                                    |                                    |
|                                    |                                   |                                    |                                    |
|                                    |                                   |                                    |                                    |
|------------------------------------|-----------------------------------|------------------------------------|------------------------------------|
|             |   |   |   |
|                                    |   |   |   |
|                                    |   |   |   |
|                                    |   |   |   |
|------------------------------------|---|---|---|
|            |   |   |   |
|                                    |   |   |   |
|                                    |   |   |   |
|                                    |   |   |   |
|------------------------------------|---|---|---|

==============================================================================================================
TEST CASE: T6 — API Contract
==============================================================================================================
| TEST                               | VALIDATION PROCESS                | TEST SUCCESS CRITERIA              | MODULES TESTED                     |
|------------------------------------|-----------------------------------|------------------------------------|------------------------------------|
| Automated Testing.                 |                                   |                                    |                                    |
|                                    |                                   |                                    |                                    |
|                                    |                                   |                                    |                                    |
|                                    |                                   |                                    |                                    |
|------------------------------------|-----------------------------------|------------------------------------|------------------------------------|
|             |   |   |   |
|                                    |   |   |   |
|                                    |   |   |   |
|                                    |   |   |   |
|------------------------------------|---|---|---|
|            |   |   |   |
|                                    |   |   |   |
|                                    |   |   |   |
|                                    |   |   |   |
|------------------------------------|---|---|---|





T1 Module Load/Unload and Interface
T1.1 Clean Load
Executes 'sudo insmod nxp_simtemp.ko'
Test Success Criteria:
This comand must be return Code 0 and dmesg from Log should not show any warning.
Modules Tested:
    nxp_simtemp_probe()
        misc_register

T1.2 Existence of Interface /dev/simtemp
Verify that '/dev/simtep' file has been created
Test Success Criteria:
After line execution of 'chmod' in run_demo.sh means that the file exist.
Modules Tested:
    misc_register

T1.3 Existence of Interface sysfs
Verify that sampling_ms, threshold_mC and stats has been created in device directory
Test Success Criteria:
Files exists and 'nxp_simtemp' driver can be unloaded cleanly (rmmod withou errors)
Modules Tested:
    syfs_create_group()
    nxp_simtemp_remove()

T2 Data Path (Telemetry Reading)

T2.1 Data Binary Integrity
Executes the continuous monitoring (python3 main.py)
Test Success Criteria:
The CLI application must be read exactly 16 bytes by sample (data packed) and unpack this sample though (<Qil) register without errors.
Modules Tested:
    nxp_simtemp_read() - Platform Driver in Kernel Module
    struct.unpack - CLI in User Space


T2.2 Visualization od Dynamic Data
Observe the data output in real time
Test Success Criteria:
The timestamp must be correct according to actual dates of the Operative System and temperature values must be variable.
Modules Tested
    simtemp_timer_callback() through: 
    ktime_get_real_ns
    get_random_bytes

T2.3 Sampling Rate Stability 
Configuration of 'sampling_ms' to 500ms (via sysfs)
Test Success Criteria
The Log of CLI must be show the samples spaced at intervals of 500 ms showing a stable monitoring.
Modules Tested
    hrtimer_forward_now()

T3 Events Validation through Threshold Alert Path
T3.1 Automated Testing
Execute run_demo.sh which calls to **cli_test_mode** this mode configure the threshold to 4000 mC and wait the alert.
Test Success Criteria
The Log resturns exit code 0 (SUCCESSFUL), verifying that poll() woke-up tith POLLPRI flag in time.
Modules Tested
    nxp_simtemp_poll() - Platform Driver in Kernel Module
    wait_queue - Platform Driver in Kernel Module
    cli_test_mode - CLI in User Space

T3.2 Alert Consumption
Execute run_demo.sh and verify the Log.
Test Success Criteria
The Log must be one line with 'alert =1' and 'KERNEL FLAGS: 3'. The ejecution of this process must be low latency and high velocity.
Modules Tested
    nxp_simtemp_read() - Platform Driver in Kernel Module


T4 Config Path and Diagnostics
T4.1 Writing and Reading with store/show modules
Use 'echo 48000'
Test Success Criteria
sudo tee threshold_mC and then cat threshold_mC
Modules Tested:
    sampling_ms_store()
    threshold_mC_show()

T4.2 stats reading
Executes cat /sys/.../stats
Test Success Criteria
The Log must be show counters for updates and alerts being integers and positives.
Modules Tested:
    stats_show()
    spin_lock




