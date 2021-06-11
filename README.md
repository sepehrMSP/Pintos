# Pintos Operating System Project

## 🚀 Project Overview
This project is an implementation of an educational operating system, **Pintos**, originally developed for **Stanford's CS 140** and later adapted by **UC Berkeley for CS 162**. The project involves enhancing the Pintos kernel by implementing functionalities such as **multithreading, user program execution, system calls, and an enhanced file system**.

## 🛠 Features Implemented
The project was divided into three major phases, each focusing on different aspects of OS development:

### 1️⃣ **User Programs & System Calls**
- Implemented system calls including `halt`, `exit`, `exec`, `wait`, `create`, `remove`, `open`, `close`, `read`, `write`, `file-size`, `tell`, and `seek`.
- Implemented **argument passing** to support command-line arguments for user programs.
- Enhanced **thread structure** to support process execution.
- Implemented **process control mechanisms** like `wait` and `exec`.
- Added proper **exit handling** to track and manage processes.
- Implemented **file descriptors** to manage opened files.

### 2️⃣ **Threads & Scheduling**
- Implemented a **priority scheduler** for better CPU utilization.
- Added **priority donation** to handle priority inversion.
- Modified **semaphores and condition variables** to support priority donation.
- Improved **synchronization mechanisms** using locks, semaphores, and condition variables.
- Implemented **Multi-Level Feedback Queue (MLFQ)** scheduler.
- Added **Shortest Remaining Time First (SRTF)** scheduling algorithm.
- Optimized **sleeping thread wakeup** mechanism.

### 3️⃣ **File System**
- Implemented **persistent file storage**.
- Extended the file system to support **directories and file descriptors**.
- Implemented **directory support**, including `mkdir`, `chdir`, and `readdir` system calls.
- Improved disk **buffer caching** for performance.
- Implemented **inode locking mechanisms** to ensure file system consistency.
- Improved **file system synchronization** with proper locking strategies.
- Implemented **file extension support** and indirect/doubly indirect block handling.
- Implemented **write protection for executable files**.


## 📂 Project Structure
```
📦 pintos
├── src
│   ├── devices      # Device drivers for timers, disks, and input handling
│   ├── examples     # Example programs for testing and demonstration
│   ├── filesys      # File system implementation with directories, inodes, and caching
│   ├── lib          # Library functions used by various components
│   ├── memory       # Memory management and paging support
│   ├── misc         # Miscellaneous utilities and helper functions
│   ├── tests        # Test cases for various components
│   ├── threads      # Core threading and synchronization primitives
│   ├── userprog     # Process execution, system calls, and user program handling
│   ├── utils        # Utility scripts and helper functions
│   └── vm           # Virtual memory (not implemented in this version)
└── README.md
```

## ⚙️ Installation & Setup
To set up and run Pintos on your local machine:

### Prerequisites
- **Linux (Ubuntu recommended) or macOS**
- **GCC, GDB, Make** (for compiling and debugging)


### ▶️ Running Pintos

```sh
cd pintos/src
make
make check  # Runs the test suite
pintos -- run <test-program>
```

## 🎯 Challenges & Learning Outcomes
- **Kernel Synchronization**: Designed thread-safe mechanisms using locks and semaphores.
- **Process Management**: Learned about memory layout, process loading, and execution.
- **File Systems**: Implemented persistent storage and file caching optimizations.
- **Low-Level Debugging**: Gained experience with **GDB, backtraces, and kernel debugging tools**.

## 📜 License
This project is for educational purposes. Feel free to fork, modify, and use it for learning!

## 🔗 References

- [Pintos Documentation](https://web.stanford.edu/class/cs140/projects/pintos/pintos_1.html)
- [CS 162 Course Materials](https://cs162.org)


---
Made with ❤️ and **a LOT of debugging** 🛠️
