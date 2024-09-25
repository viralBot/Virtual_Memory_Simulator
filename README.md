# Operating Systems Virtual Memory Simulation

## Project Overview
This project simulates a demand-paged virtual memory system in a multiprogramming environment. It demonstrates key aspects of virtual memory management, including page table management, page fault handling, and page replacement using the Least Recently Used (LRU) algorithm.

## Components
1. Master: Initializes data structures and spawns other modules.
2. Scheduler: Schedules processes using First-Come, First-Served (FCFS) algorithm.
3. Memory Management Unit (MMU): Translates virtual addresses to physical addresses and handles page faults.
4. Processes: Generate page reference strings and interact with the MMU.

## Input
- Total number of processes (k)
- Virtual address space - Maximum number of pages per process (m)
- Physical address space - Total number of frames (f)

## Implementation Details
- Master: Creates data structures, scheduler, MMU, and processes.
- Scheduler: Manages process scheduling using FCFS.
- Processes: Generate page numbers and communicate with MMU.
- MMU: Handles address translation, page faults, and page replacement.

## Data Structures
- Page Table: Shared memory, one per process.
- Free Frame List: Shared memory, maintained by MMU.
- Ready Queue: Message queue for scheduler.
- Message Queues: For inter-module communication.

## Output
The MMU prints to an xterm window and writes to "result.txt":
- Page fault sequence
- Invalid page references
- Global ordering of page references
