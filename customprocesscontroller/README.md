# Process Monitoring and Management System (PMMS)

## 📌 Overview
This project is a simple Process Monitoring and Management System built using **C** and **Bash scripting** as part of an Operating Systems course assignment.

It demonstrates process creation, signal handling, and basic shell-based interaction with background processes.

---

## 🚀 Features
- Spawns **3 child processes** using `fork()` in C
- Child processes:
  - Print their PID and a message every 3 seconds
  - Pause/Resume on `SIGUSR1`
  - Terminate gracefully on `SIGTERM`
- Parent process:
  - Logs creation and termination of children
  - Handles `SIGINT` (Ctrl+C) to terminate all children
- Bash shell script (`pmms-monitor.sh`) provides interactive control over the processes

---

## 🧾 Files
- `pmms.c`: Core C program that manages process creation and signal handling
- `pmms-monitor.sh`: Interactive Bash script to monitor and control the processes
- `Makefile`: Automates compilation, execution, and cleanup
- `pmms.log`: Log file that records process events (auto-generated)

---

## 🛠️ Usage
### Step 1: Make executable
```bash
chmod +x pmms-monitor.sh
```

### Step 2: Run using Makefile
```bash
make        # Compiles the C program
make run    # Launches the monitor script
```

### Menu Options
- View child process status
- Pause/Resume a child process
- Kill a specific child process
- Kill all child processes
- View the latest log output
- Exit the monitor

### Step 3: Cleanup
```bash
make clean  # Deletes compiled files
```

---

## 🧠 Concepts Practiced
- Multitasking using `fork()`, `execvp()`, `wait()`
- UNIX signal handling (`SIGUSR1`, `SIGTERM`, `SIGINT`)
- Inter-process communication and process management
- Bash scripting for system monitoring

---

## 📷 Sample Output
```
Parent PID: 12345
[Child 12346] Active ...
[Child 12347] Active ...
[Child 12348] Active ...
^C
[Parent] All children terminated. Exiting now.

---

## 👨‍💻 Author
- Abdul kalam
- National University of Computer and Emerging Sciences

---

## 📄 License
This project is for educational purposes only.
