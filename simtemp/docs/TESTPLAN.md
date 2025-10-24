# NXP-Systems-Software-Engineer-Candidate-Challenge
# Welcome to NXP Systems Software Engineer Candidate Challenge!

# TESTPLAN.md

# 1. Objectives and scopes
The objective of this Test Plan is validate the behavior and robustness of Platform Driver Module 'nxp_simtemp' and its User Space Interface (CLI). The scope covers three communication routes: Data Control and Asyncronous Events.

# 2. Test Environment

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
| TEST CASE: T1 — Load/Unload                                                                                                                      |
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
| TEST CASE: T2 — Periodic Read                                                                                                                    |
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


====================================================================================================================================================
| TEST CASE: T3 — Threshold Event                                                                                                                  |
====================================================================================================================================================
| TEST                               | VALIDATION PROCESS                | TEST SUCCESS CRITERIA              | MODULES TESTED                     |
|------------------------------------|-----------------------------------|------------------------------------|------------------------------------|
| Automated Testing.                 | Execute run_demo.sh which calls   | The Log returns exit 'code 0'      | nxp_simtemp_poll()                 |
|                                    | to 'cli_test_mode' this mode      | (SUCCESSFUL), verifying that poll()| wait_queue                         |
|                                    | configure the threshold to        | woke-up with POLLPRI flag in time. | cli_test_mode                      |
|                                    | 4000 mC and wait the alert.       |                                    |                                    |
|------------------------------------|-----------------------------------|------------------------------------|------------------------------------|
| Alert Consumption.                 | Execute run_demo.sh and verify    | The Log must be one line with      | nxp_simtemp_read()                 |
|                                    | the Log.                          | 'alert =1' and 'KERNEL FLAGS: 3'.  |                                    |
|                                    |                                   | The ejecution of this process      |                                    |
|                                    |                                   | must be low latency and high       |                                    |
|                                    |                                   | velocity.                          |                                    |
|------------------------------------|-----------------------------------|------------------------------------|------------------------------------|


====================================================================================================================================================
| TEST CASE: T4 — Error Paths                                                                                                                      |
====================================================================================================================================================
| TEST                               | VALIDATION PROCESS                | TEST SUCCESS CRITERIA              | MODULES TESTED                     |
|------------------------------------|-----------------------------------|------------------------------------|------------------------------------|
| Writing and Reading with           | Use 'echo 48000'                  | sudo tee threshold_mC              | sampling_ms_store()                |
| store/show modules                 |                                   | and then cat threshold_mC          | threshold_mC_show()                |
|                                    |                                   |                                    |                                    |
|                                    |                                   |                                    |                                    |
|------------------------------------|-----------------------------------|------------------------------------|------------------------------------|
| stats reading                      | Executes cat /sys/.../stats       | The Log must be show counters for  | stats_show()                       |
|                                    |                                   | updates and alerts being integers  | spin_lock                          |
|                                    |                                   | and positives.                     |                                    |
|                                    |                                   |                                    |                                    |
|------------------------------------|-----------------------------------|------------------------------------|------------------------------------|


====================================================================================================================================================
| TEST CASE: T5 — Concurrency                                                                                                                      | 
====================================================================================================================================================
| TEST                               | VALIDATION PROCESS                | TEST SUCCESS CRITERIA              | MODULES TESTED                     |
|------------------------------------|-----------------------------------|------------------------------------|------------------------------------|
|  Clean unload.                     | Execute in bash:                  |  Execute in bash:                  | nxp_simtemp_remove()               |
|                                    | 'sudo insmod nxp_simtemp.ko'      |  'lsmod'                           | misc_deregister                    |
|                                    | After this execute                |  After this, the module            | sysfs_remove_group()               |
|                                    | 'sudo insmod nxp_simtemp'         |  nxp_simtemp should not be found   |                                    |
|------------------------------------|-----------------------------------|------------------------------------|------------------------------------|
| Atomicity in hrtimer               | Execute in bash:                  | The Driver must not be fall in     | sampling_ms_store()                |
|                                    | 'pyhton3 main.py'                 | Kernel Panic or deadlock.          | hrtimer_cancel                     | 
|                                    | Executes a script that write a    | The monitoring must be continue in | simtemp_timer_callback()           |
|                                    | new value in 'sampling_ms' 100    | reading until the last rate        | spinlock                           |
|                                    | times in a fast loop.             | configured.                        |                                    |
|------------------------------------|-----------------------------------|------------------------------------|------------------------------------|
|  Multiprocess Access               | Execute in bash:                  | Concurrency un both processes      | nxp_simtemp_read()                 |
|                                    | 'sudo insmod nxp_simtemp.ko'      | without fails and errors, duplcated| simtemp_buffer_pop()               |
|                                    | Execute two instances separated   | data and invalid readings that     | spinlock                           |
|                                    | from the process.                 | could break the atomicity in       |                                    |
|                                    | python3 monitor.py and            | struct.unpack.                     |                                    |
|                                    | python3 monitor.py simultaneously |                                    |                                    |
|------------------------------------|-----------------------------------|------------------------------------|------------------------------------|


====================================================================================================================================================
| TEST CASE: T6 — API Contract                                                                                                                     |
====================================================================================================================================================
| TEST                               | VALIDATION PROCESS                | TEST SUCCESS CRITERIA              | MODULES TESTED                     |
|------------------------------------|-----------------------------------|------------------------------------|------------------------------------|
| Binary Consistency                 | Implementation of debugging mode  | The CLO must be read the sample and| nxp_simtemp_read()                 |
|                                    | to fill the Ring Buffer with      | report the numeric values without  | struct.unpack                      |
|                                    | 0xFFFFFFFF in every value.        | parsing errors and alingment       |                                    |
|                                    |                                   |                                    |                                    |
|                                    | Run in bash:                      |                                    |                                    |
|                                    | 'python 3 main.py'                |                                    |                                    |
|------------------------------------|-----------------------------------|------------------------------------|------------------------------------|
| Short Reading Handling             | Implementing changes in 'main.py' | The Driver returns only the size   | nxp_simtemp_read()                 |
|                                    | in data = os.read(fd, SAMPLE_SIZE)| requested and log the values in CLI| CLI (I/O handling)                 |
|                                    | with new values                   | and fail with -EINVAL for          |                                    |
|                                    | for data = os.read(fd, 8)         | telemetry reading.                 |                                    |
|------------------------------------|-----------------------------------|------------------------------------|------------------------------------|
| sysfs Sanity Check                 | After load the driver nxp_simtemp | The Log bash return for all cases: | kstrtoul                           |
| Invalide Control Reading           | Execute in bash:                  | sudo tee /sys/.../sampling_ms      |                                    |
|                                    | 'echo "abc"'                      |                                    |                                    |
|                                    | 'echo -10'                        | and returns -EINVAL                |                                    |
|                                    | 'echo  5'                         |                                    |                                    |
|------------------------------------|-----------------------------------|------------------------------------|------------------------------------|