# FOS (FOS Operating System) - OS'25 Project

## 📋 Project Overview

A complete operating system kernel implementation featuring advanced memory management, process scheduling, and synchronization primitives. This project demonstrates core OS concepts including dynamic memory allocation, page fault handling with multiple replacement algorithms, inter-process communication, and priority-based scheduling.

---

## 🏗️ System Architecture

### Module Dependencies

```
Dynamic Allocator ──┐
Kernel Heap        ├──→ Fault Handler I (Placement) ──→ Advanced Features
Fault Handler I    ──┘                                    │
                                                          ├──→ Fault Handler II (Replacement)
                                                          ├──→ User Heap
                                                          ├──→ Shared Memory
                                                          ├──→ CPU Scheduling
                                                          └──→ Kernel Protection
```

---

## ✅ Implemented Features

### 1. Dynamic Allocator

**Purpose:** Efficient memory allocation for small-sized blocks

**Implementation Highlights:**

- Power-of-2 block allocation (8B to 2KB)
- O(1) best case allocation performance
- Free block management using linked lists
- Page-level memory tracking
- Automatic page allocation and deallocation

**Key Components:**

- `initialize_dynamic_allocator()` - Allocator initialization with configurable memory range
- `get_block_size()` - Block size retrieval by virtual address
- `alloc_block()` - Smart block allocation with fallback strategies
- `free_block()` - Block deallocation with automatic page return

**Data Structures:**

- Free block lists organized by size (8B, 16B, 32B, ..., 2KB)
- Page info array tracking block size and free count per page
- Free pages list for quick page allocation

---

### 2. Kernel Heap

**Purpose:** Dynamic memory allocation for kernel operations

**Implementation Highlights:**

- Dual allocator architecture:
  - **Block Allocator** (≤ 2KB): Leverages dynamic allocator
  - **Page Allocator** (> 2KB): Page-boundary allocation with CUSTOM FIT strategy
- Virtual memory management without 1-1 physical mapping
- Efficient address translation (O(1) complexity)
- Memory range: `[KERNEL_HEAP_START, KERNEL_HEAP_MAX)`

**Key Components:**

- `kmalloc()` - Kernel memory allocation with size-based strategy selection
- `kfree()` - Memory deallocation with boundary merging
- `kheap_physical_address()` - Fast VA to PA translation
- `kheap_virtual_address()` - Fast PA to VA translation
- `krealloc()` - Memory reallocation with optimization (Bonus)

**CUSTOM FIT Algorithm:**

1. Search for exact fit in free spaces
2. Fallback to worst fit if no exact match
3. Extend break pointer if space available
4. Return NULL if allocation impossible

**Advanced Features:**

- Adjacent block merging on free
- Automatic break pointer management
- Guard page protection for kernel stacks

---

### 3. Fault Handler I (Placement)

**Purpose:** Handle page faults and load pages from disk to memory

**Implementation Highlights:**

- Comprehensive fault detection (table faults and page faults)
- Page file integration for on-demand loading
- Working set management for active pages
- Invalid pointer detection and process protection

**Key Components:**

- `create_user_kern_stack()` - Kernel stack creation with guard pages
- `fault_handler()` - Invalid access validation and rejection
- `page_fault_handler()` - Page loading and working set updates

**Fault Handling Logic:**

1. Validate fault address (heap marking, kernel access, permissions)
2. Allocate physical frame for faulted page
3. Load page content from page file
4. Handle special cases (stack/heap pages without backing)
5. Update working set list with new page

---

### 4. Fault Handler II (Replacement Algorithms)

**Purpose:** Intelligent page replacement when memory is full

**Implemented Algorithms:**

#### OPTIMAL Algorithm (Benchmark)

- Reference stream tracking during execution
- Offline optimal replacement calculation
- Future reference prediction
- Active working set management to prevent infinite faults

**Features:**

- `get_optimal_num_faults()` - Trace-based fault counting
- Reference stream list maintenance
- Present bit manipulation for tracking

#### CLOCK Algorithm (Second Chance)

- Circular scanning with "used bit"
- Second-chance mechanism for recently accessed pages
- Efficient victim selection

#### LRU (Aging) Algorithm

- 32-bit counter per page
- Clock tick-based aging mechanism
- Automatic counter shift and used bit integration
- `update_WS_time_stamps()` - Counter updates on every clock tick

#### Modified CLOCK Algorithm

- Two-bit state tracking (used, modified)
- Two-pass victim selection:
  - **Pass 1:** Find (0,0) - clean, unused pages
  - **Pass 2:** Find (0,x) - unused pages, clearing used bits
- Write optimization by preferring clean pages

**Runtime Switching:**

```bash
FOS> optimal     # OPTIMAL algorithm
FOS> clock       # CLOCK algorithm
FOS> lru         # LRU aging algorithm
FOS> modclock    # Modified CLOCK algorithm
```

---

### 5. User Heap

**Purpose:** Dynamic memory allocation for user processes

**Implementation Highlights:**

- Dual allocator system matching kernel heap architecture
- Memory ranges:
  - Block Allocator: `[USER_HEAP_START, BLK_ALLOC_LIMIT)`
  - Page Allocator: `[BLK_ALLOC_LIMIT + 4KB, USER_HEAP_MAX)`
- CUSTOM FIT strategy for page allocation
- Lazy allocation via page fault mechanism
- Page marking with PERM_UHPAGE permission

**Key Components:**

**User-Side Functions:**

- `malloc()` - User memory allocation with automatic strategy selection
- `free()` - Memory deallocation with space merging

**Kernel-Side Functions:**

- `allocate_user_mem()` - Page reservation and marking
- `free_user_mem()` - Page unmarking, page file cleanup, working set removal

**Allocation Flow:**

1. Size-based allocator selection (block vs page)
2. CUSTOM FIT search for suitable space
3. Kernel-side page marking/allocation
4. On-demand physical allocation via page faults

---

### 6. Shared Memory

**Purpose:** Inter-process communication through shared memory regions

**Implementation Highlights:**

- Frame-level sharing between processes
- Permission control (read-only/writable)
- Reference counting for shared objects
- CUSTOM FIT allocation in page allocator area
- Frame storage tracking for reuse

**Key Components:**

**User-Side Functions:**

- `smalloc()` - Create and allocate shared memory object
- `sget()` - Access existing shared object from another process

**Kernel-Side Functions:**

- `alloc_share()` - Share object structure allocation
- `create_shared_object()` - Physical frame allocation and tracking
- `get_shared_object()` - Frame sharing with permission enforcement

**Data Structures:**

- Global shares list with lock protection
- Share object metadata (ID, name, owner, size, permissions)
- Frame storage array for tracking allocated frames
- Reference counter for cleanup management

**Usage Example:**

```c
// Process 1: Create shared memory
int* shared_data = smalloc("data", 4096, 1);  // writable
*shared_data = 42;

// Process 2: Access shared memory
int* ptr = sget(process1_ID, "data");
int value = *ptr;  // value = 42
```

**Protection:** All shared memory operations protected with kernel-level locks

---

### 7. CPU Scheduling (Priority Round Robin)

**Purpose:** Fair CPU time distribution with priority support

**Implementation Highlights:**

- Multiple priority queues for different priority levels
- Round-robin scheduling within each priority
- Starvation prevention via aging mechanism
- Preemptive scheduling on clock interrupts
- Configurable quantum and priorities

**Key Components:**

- `env_set_priority()` - Dynamic priority assignment
- `sched_set_starv_thresh()` - Starvation threshold configuration
- `sched_init_PRIRR()` - Scheduler initialization
- `fos_scheduler_PRIRR()` - Next process selection
- `clock_interrupt_handler()` - Aging and promotion logic

**Scheduling Algorithm:**

1. Select highest-priority non-empty queue
2. Round-robin selection within priority level
3. Monitor process age (ticks waiting)
4. Promote processes exceeding starvation threshold
5. Context switch with quantum reset

**Commands:**

```bash
FOS> run <prog> <ws_size> [<priority>]      # Run with priority
FOS> setPri <envID> <priority>               # Change priority
FOS> setStarvThr <threshold>                 # Set starvation threshold
FOS> schedPRIRR <#pri> <quantum> <thresh>   # Configure scheduler
FOS> schedRR <quantum>                       # Switch to RR
```

---

### 8. Kernel Protection (Synchronization Primitives)

**Purpose:** Thread-safe kernel operations and resource protection

#### Sleep Locks

**Implementation Highlights:**

- Non-busy-wait locking mechanism
- Channel-based sleep/wakeup
- Spinlock protection for queue operations
- Suitable for long critical sections

**Key Components:**

- `sleep()` - Block process on channel with lock release
- `wakeup_one()` - Wake single blocked process
- `wakeup_all()` - Wake all blocked processes on channel
- `acquire_sleeplock()` - Acquire with automatic blocking
- `release_sleeplock()` - Release with wakeup

**Implementation Details:**

- Guard spinlock prevents race conditions
- Queue protection during sleep/wakeup
- Lock reacquisition on wakeup
- While-loop for condition rechecking

#### Semaphores

**Implementation Highlights:**

- Counting semaphores for resource management
- Atomic wait/signal operations
- FIFO blocking queue
- No busy-waiting
- Deadlock prevention in priority inversion

**Key Components:**

- `wait_ksemaphore()` - Decrement counter, block if negative
- `signal_ksemaphore()` - Increment counter, wakeup if needed

**Semaphore Operations:**

```c
// Critical section protection
semaphore mutex = 1;
wait_ksemaphore(&mutex);
// ... critical section ...
signal_ksemaphore(&mutex);

// Synchronization/dependency
semaphore sync = 0;
// Thread 1
signal_ksemaphore(&sync);  // Signal completion

// Thread 2
wait_ksemaphore(&sync);    // Wait for signal
```

**Advantages:**

- No busy-waiting (CPU efficient)
- Multi-processor support
- Multiple critical sections
- FIFO ordering prevents starvation
- Priority inversion handling

---

## 🔧 System Configuration

### Memory Layout

```
Virtual Memory Map:                                    Permissions
                                                       kernel/user

4 GB --------->  +------------------------------+
                 |   Invalid Memory (*)         | PAGE_SIZE
KERNEL_HEAP_MAX  +------------------------------+
                 |   Kernel Heap (KHEAP)        | RW/--
                 :              .               :
                 :              .               :
KERNEL_HEAP_START+------------------------------+ 0xf6000000
                 :              .               :
                 :              .               :
                 |~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~| RW/--
                 |                              | RW/--
                 | Remapped Physical Memory     | RW/--
                 |                              | RW/--
KERNEL_BASE -->  +------------------------------+ 0xf0000000
                 | Cur. Page Table (Kern. RW)   | RW/-- PTSIZE
VPT ---------->  +------------------------------+ 0xefc00000
SCHD_KERN_       | Sched Kernel Stack CPU0      | RW/-- KERNEL_STACK_SIZE
STACK_TOP        | Sched Kernel Stack CPU1      | RW/-- KERNEL_STACK_SIZE
                 + ...                          +
                 |                              | PTSIZE
                 | Invalid Memory (*)           | --/--
                 |                              |
USER_LIMIT --->  +------------------------------+ 0xef800000
                 | Cur. Page Table (User R-)    | R-/R- PTSIZE
UVPT --------->  +------------------------------+ 0xef400000
                 | FREE Space                   | PTSIZE
                 +------------------------------+ 0xef000000
                 | RO ENVS                      | R-/R- PTSIZE
USER_TOP,        +------------------------------+ 0xeec00000
UENVS -------->  | User Exception Stack         | RW/RW PAGE_SIZE
UXSTACKTOP       +------------------------------+ 0xeebff000
                 | Invalid Memory (*)           | --/-- PAGE_SIZE
USTACKTOP --->   +------------------------------+ 0xeebfe000
                 | Normal User Stack            | RW/RW PAGE_SIZE
                 +------------------------------+ 0xeebfd000
                 .                              .
                 .                              .
                 .                              .
USTACKBOTTOM,    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
USER_PAGES_      |                              |
WS_MAX           | User Pages Working Set       |
USER_HEAP_MAX    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 0xa0000000
USER_PAGES_      .                              .
WS_START         .                              .
                 .    User Heap                 .
USER_HEAP_START  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 0x80000000
                 .                              .
                 .                              .
                 .                              .
                 | Program Code & Data          |
UTEXT -------->  +------------------------------+ 0x00800000
PFTEMP ------->  | Empty Memory (*)             | PTSIZE
                 |                              |
UTEMP -------->  +------------------------------+ 0x00400000
PGFLTEMP ----->  | PAGE_SIZE                    |
                 | Empty Memory (*)             |
                 | - - - - - - - - - - - - - - -|
                 | User STAB Data (optional)    | PTSIZE
USTABDATA ---->  +------------------------------+ 0x00200000
                 | Empty Memory (*)             |
0 ------------>  +------------------------------+
```

### Configuration Options

Enable kernel heap in `inc/memlayout.h`:

```c
#define USE_KHEAP 1
```

---

## 📊 Performance Characteristics

| Component                 | Complexity            | Notes                       |
| ------------------------- | --------------------- | --------------------------- |
| Dynamic Allocator (alloc) | O(1) best, O(C) worst | C = max blocks per page     |
| Dynamic Allocator (free)  | O(1) avg, O(K) worst  | K = free blocks in list     |
| Kernel Heap PA↔VA         | O(1)                  | Hash-based translation      |
| Page Allocator            | O(N)                  | N = free spaces, CUSTOM FIT |
| CPU Scheduler             | O(P)                  | P = number of priorities    |
| Sleep/Wakeup              | O(1)                  | Queue operations            |

---

## 📁 Project Structure

```
kern/
├── mem/
│   ├── kheap.c                    # Kernel heap implementation
│   ├── chunk_operations.c         # User heap kernel operations
│   └── shared_memory_manager.c    # Shared memory management
├── trap/
│   └── fault_handler.c            # Page fault handling & replacement
├── cpu/
│   ├── sched.c                    # Priority RR scheduler
│   └── sched_helpers.c            # Scheduler utilities
└── conc/
    ├── channel.c                  # Sleep/wakeup channels
    ├── sleeplock.c                # Sleep locks implementation
    └── ksemaphore.c               # Kernel semaphores

lib/
├── dynamic_allocator.c            # Block-level allocator
└── uheap.c                        # User heap (malloc/free)

inc/
├── dynamic_allocator.h
├── memlayout.h
├── environment_definitions.h
├── uheap.h
└── conc/
    ├── sleeplock.h
    ├── ksemaphore.h
    └── channel.h
```

---

## 🧪 Testing & Usage

### Basic Commands

```bash
# Process management
FOS> run <program> <ws_size> [<priority>]   # Load and run program
FOS> load <program> <ws_size> [<priority>]  # Load program
FOS> runall                                  # Run all loaded programs
FOS> printall                                # Display all processes
FOS> sched?                                  # Show scheduler info

# Priority management
FOS> setPri <envID> <priority>              # Set process priority
FOS> setStarvThr <threshold>                # Set starvation threshold

# Scheduler configuration
FOS> schedPRIRR <#pri> <quant> <thresh>     # Priority RR scheduler
FOS> schedRR <quantum>                       # Round Robin scheduler

# Page replacement algorithms
FOS> optimal                                 # OPTIMAL replacement
FOS> clock                                   # CLOCK replacement
FOS> lru                                     # LRU aging replacement
FOS> modclock                                # Modified CLOCK replacement
```

---

## 🚀 System Capabilities

- ✅ Dynamic memory allocation (kernel and user space)
- ✅ Demand paging with multiple replacement algorithms
- ✅ Inter-process communication via shared memory
- ✅ Priority-based preemptive scheduling
- ✅ Thread synchronization primitives
- ✅ Memory protection and fault recovery
- ✅ Efficient address space management
- ✅ Lock-based concurrency control

---

**FOS Operating System - A complete educational OS implementation demonstrating modern operating system concepts**
