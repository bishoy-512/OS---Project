# 🖥️ FOS - Operating System Implementation

A comprehensive operating system project implementing core OS concepts including memory management, CPU scheduling, and page replacement algorithms.

## 📋 Table of Contents
- [Features](#features)
- [Memory Management](#memory-management)
- [CPU Scheduling](#cpu-scheduling)
- [Page Replacement](#page-replacement)
- [Getting Started](#getting-started)
- [Project Structure](#project-structure)
- [Technologies Used](#technologies-used)

## ✨ Features

### Memory Management
- **User Heap Management**: Dynamic memory allocation for user processes
- **Kernel Heap (kheap)**: Efficient kernel-level memory allocation
- **Shared Memory**: Inter-process communication via shared memory segments
- **Kernel Protection**: Memory protection mechanisms to ensure system stability
- **Dynamic Allocation**: Runtime memory allocation and deallocation

### CPU Scheduling
Implementation of various CPU scheduling algorithms to optimize process execution and system performance.

### Page Replacement Algorithms
- **LRU (Least Recently Used)**: Replaces the least recently accessed page
- **Clock Algorithm**: Circular buffer-based page replacement
- **Modified Clock**: Enhanced clock algorithm with additional reference bits
- **Optimal**: Theoretical optimal page replacement for comparison

## 🧠 Memory Management

The memory management system handles both user and kernel space memory efficiently:

```
┌─────────────────────────────────┐
│      User Space Memory          │
│  - User Heap                    │
│  - Shared Memory Segments       │
└─────────────────────────────────┘
┌─────────────────────────────────┐
│     Kernel Space Memory         │
│  - Kernel Heap (kheap)          │
│  - Protected Kernel Data        │
└─────────────────────────────────┘
```

## ⚙️ CPU Scheduling

The scheduler manages process execution with support for multiple scheduling policies to balance system throughput and response time.

## 📄 Page Replacement

Implemented page replacement algorithms for virtual memory management:

| Algorithm | Description | Use Case |
|-----------|-------------|----------|
| **Optimal** | Theoretical best performance | Benchmark comparison |
| **LRU** | Practical and efficient | General purpose |
| **Clock** | Simple and fast | Resource-constrained systems |
| **Modified Clock** | Enhanced accuracy | Better performance trade-off |

## 🚀 Getting Started

### Prerequisites
- GCC compiler
- Make
- QEMU (for testing)

### Building the Project

```bash
# Clone the repository
git clone <repository-url>
cd fos-project

# Build the project
make

# Run in QEMU
make qemu
```

### Running Tests

```bash
# Run all tests
make test

# Run specific component tests
make test-memory
make test-scheduling
make test-paging
```

## 📁 Project Structure

```
fos-project/
├── kern/           # Kernel source code
│   ├── mem/        # Memory management
│   ├── cpu/        # CPU scheduling
│   └── trap/       # Interrupt handling
├── lib/            # Shared libraries
├── user/           # User space programs
└── inc/            # Header files
```

## 🛠️ Technologies Used

- **Language**: C
- **Architecture**: x86
- **Build System**: Make
- **Emulator**: QEMU

## 📚 Key Concepts Implemented

- Virtual Memory Management
- Process Scheduling
- Page Fault Handling
- Memory Protection
- Dynamic Memory Allocation
- Inter-Process Communication

## 🤝 Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues.

## 📝 License

This project is part of an operating systems course implementation.

---

**Note**: This is an educational operating system project developed for learning core OS concepts.
