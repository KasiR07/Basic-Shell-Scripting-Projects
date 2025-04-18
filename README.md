Distributed File System DFS using Linux


# Distributed File System (DFS) Project – COMP 8567

This project is a client-server distributed file system simulation implemented in C using socket programming. It allows multiple clients to interact with four backend servers (S1 to S4), supporting common file operations such as uploading, downloading, removing, listing, and creating tar archives.

---

## 📘 Objective

Simulate a distributed file system architecture with command parsing, file type routing, and inter-server communication. This project demonstrates:
- Inter-process communication using sockets
- Multi-process concurrency using `fork()`
- File I/O and directory management
- Command validation (both client- and server-side)
- Robust automated testing and modular structure

---

## 🧩 Project Components

### Servers:
- **S1 (Main server)**: Listens to clients, handles `.c` files locally, and routes other file types to appropriate servers.
- **S2**: Stores `.pdf` files
- **S3**: Stores `.txt` files
- **S4**: Stores `.zip` files

### Client:
- Interacts only with S1
- Validates syntax of each command before sending
- Sends/receives file data and metadata

---

## 🗂️ Files and Directories

```
.
├── updated_S1.c             # Server 1: main dispatcher and handler
├── updated_S2.c             # Server 2: receives and stores PDF files
├── updated_S3.c             # Server 3: receives and stores TXT files
├── updated_S4.c             # Server 4: receives and stores ZIP files
├── updated_w25clients.c     # Client program to communicate with S1
├── updated_test_operations.sh # Script to test all core features
├── README.md                # Documentation
```

---

## 🛠️ Compilation Instructions

Compile all C source files:

```bash
gcc updated_S1.c -o updated_S1
gcc updated_S2.c -o updated_S2
gcc updated_S3.c -o updated_S3
gcc updated_S4.c -o updated_S4
gcc updated_w25clients.c -o updated_w25clients
```

---

## 🚀 Running the Project

You can either manually run each binary or use the provided automated test script.

### 🔹 Manual Execution

```bash
./updated_S1 9000 9001 9002 9003
./updated_S2 9001
./updated_S3 9002
./updated_S4 9003
./updated_w25clients 9000
```

### 🔹 Automated Testing

```bash
chmod +x updated_test_operations.sh
./updated_test_operations.sh
```

This will:
- Start all servers
- Create test files
- Execute upload, download, delete, list, and tar operations
- Validate results
- Shut down all servers

---

## 🧪 Commands Supported by Client

| Command | Example | Description |
|--------|---------|-------------|
| `uploadf <filename> [destination_path]` | `uploadf test.pdf ~/S2/reports` | Upload a file. If it's `.c`, stored on S1; others routed. |
| `downlf <filename>` | `downlf ~/S3/docs/file.txt` | Download a file to client directory |
| `removef <filename>` | `removef ~/S4/archive/test.zip` | Remove a file from its respective server |
| `downltar <filetype>` | `downltar txt` | Creates and downloads a tarball of all `.txt` files |
| `dispfnames [path]` | `dispfnames ~/S2/reports` | Lists all files in a given directory |
| `exit` | | Exits the client program |

---

## 🧠 Key Features

### ✅ Client-side Syntax Validation
Ensures commands are correct before sending to S1. Prints helpful error messages for missing arguments or unknown commands.

### ✅ Full Path Handling
Supports deeply nested file operations like:
```bash
uploadf report.pdf ~/S2/yearly/2025/April
removef ~/S2/yearly/2025/April/report.pdf
```

### ✅ Tarball Creation
Servers dynamically generate `.tar` files containing all files of a given type.

### ✅ Robust Testing
Automated test script verifies:
- Upload/download functionality
- Directory creation
- File listing
- Tar file integrity
- Full and partial path removal

---

## 🧹 Cleanup

To reset the system, just run:
```bash
./updated_test_operations.sh
```
It clears all test directories and kills running server instances.

---

## ⚠️ Known Assumptions

- Maximum path length is limited by buffer size (1024 bytes)
- Supports basic file types only: `.c`, `.pdf`, `.txt`, `.zip`
- Tarball operation doesn't support `.zip` type as per spec

---

## 📍 Developed By

- **Naga Venkata Sai Ruthvik Kasi**
- Master's in Applied Computing (AI Specialization)
- University of Windsor
- COMP-8567 Advanced Systems Programming

---

## 📞 Contact & Feedback

For bugs, suggestions, or improvements, feel free to connect or raise an issue.

