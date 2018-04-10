# README

## Introduction

This is the skeleton code for the 2017 Fall ECE408 / CS483 course project.
In this project, you will get experience with practical neural network artifacts, face the challenges of modifying existing real-world code, and demonstrate command of basic CUDA optimization techniques.
Specifically, you will get experience with

* Basic OS knowledge:

1,Process states: New, Ready, Running, Waiting, Terminated. Process control block: process ID, process states, CPU register and Program counter, CPU scheduling information, memory management information, accounting information, I/O status information.

2,Processing scheduler: Non pre-emptive scheduling. When the currently executing process gives up the CPU voluntarily. Pre-emptive scheduling. When the operating system decides to favour another process, pre-empting the currently executing process.

3,Scheduling queue: job queue, ready queue, device queue.

4,Scheduler: long term scheduler, CPU scheduler(short term scheduler), Medium Term Scheduler: During extra load, this scheduler picks out big processes from the ready queue for some time, to allow smaller processes to execute, thereby reducing the number of processes in the ready queue.

5,CPU scheduling criteria:
  5.1,CPU utilization: To make out the best use of CPU and not to waste any CPU cycle, CPU would be working most of the time(Ideally 100% of the time). Considering a real system, CPU usage should range from 40% (lightly loaded) to 90% (heavily loaded.)
  5.2,Throughput: It is the total number of processes completed per unit time or rather say total amount of work done in a unit of time. This may range from 10/second to 1/hour depending on the specific processes.
  5.3,Turnaround time: It is the amount of time taken to execute a particular process, i.e. The interval from time of submission of the process to the time of completion of the process(Wall clock time).
  5.4,Waiting time: The sum of the periods spent waiting in the ready queue amount of time a process has been waiting in the ready queue to acquire get control on the CPU.
  5.5,Load average: It is the average number of processes residing in the ready queue waiting for their turn to get into the CPU.
  5.6,Response time: Amount of time it takes from when a request was submitted until the first response is produced. Remember, it is the time till the first response and not the completion of process execution(final response).
In general CPU utilization and Throughput are maximized and other factors are reduced for proper optimization.

6,CPU scheduling algorithm: First Come first served; shortest job first; Priority Scheduling( the priority can be decided based on memory requirements, time requirements or any other resource requirement). Round Robin Scheduling (A fixed time is allotted to each process, called quantum); Multilevel Queue Scheduling ( Multiple queues are maintained for processes. Each queue can have its own scheduling algorithms. Priorities are assigned to each queue.)

7,Thread: Thread is an execution unit which consists of its own program counter, a stack, and a set of registers. User thread, and kernel thread. Many user thread have to be mapped to kernel threads.

8,Thread library: provides programmers with API for creating and managing of threads. POSIX-Pithread; win32threads; Java threads.

9,Multithreading Issues:
  9.1,Thread Cancellation: Asynchronous cancellation, which terminates the target thread immediately; Deferred cancellation allows the target thread to periodically check if it should be cancelled.
  9.2,Signal sharing
  9.3,Fork() system call
  9.4,Security issues
  
10,Critical section: (code segment that accesses shared variables) solutions: 1) Mutual exclusion. 2) Progress 3) Bounded waiting.

11,Semaphores: an integer variable for synchronization. Wait, signal. Type: binary semaphore, counting semaphore.

12,Classical Problem of Synchronization
  12.1,Bounded buffer problem, also as Producer-customer problem.The producer and the consumer, who share a common, fixed-size buffer used as a queue. Solution: 2 semaphores: full and empty.
  12.2,The Readers Writers Problem
  12.3,Dining Philosophers Problem: the allocation of limited resources from a group of processes in a deadlock-free and starvation-free manner.

13,Deadlock: to avoid: mutual exclusion; hold and wait; no preemption; circular wait. Handling: Preemption (take a resource from one process and give it to other); rollback; kill one or more process.

14,System call: user mode (if one program crashes, only that particular program is halted. Does not directly access memory and hardware resource. Kernel mode. When a program in user mode requires access to RAM or a hardware resource, it must ask the kernel to provide access to that resource. This is done via something called a system call. When a program makes a system call, the mode is switched from user mode to kernel mode. This is called a context switch.

15,Example system call: The fork() call creates a new process while preserving the parent process. But, an exec() call replaces the address space, text segment, data segment etc. of the current process with the new process.



##Memory System Overview
16,Locality:temporal and spatial locality.
Temporal locality refers to the reuse of specific data, and/or resources, within a relatively small time duration. Spatial locality refers to the use of data elements within relatively close storage locations

17,Cache mapping: direct mapping, set associate

![image](http://github.com/jiayuewang/Manually-implemented-CPU-cs423/raw/master/image/1.png)

![image](http://github.com/jiayuewang/Manually-implemented-CPU-cs423/raw/master/image/2.png)

18,Cache writing policy:

Cache hit: write-back (only write to cache); write throughout
Cache miss: write allocate, no-write allocate(write directly to memory)


19,Virtual address abstraction:

    Advantages:
    
    1 have more memory
    
    2 Protection for application data
    
    3 Sharing: Application data can share address
    
    4 Sharing
    
Page table: Virtual address -> physical address. translation lookaside buffer (TLB) first, if miss, look into page table.

Global page table: kernel address has the same for each process, they use (GPT) Global Page Table.
![image](http://github.com/jiayuewang/Manually-implemented-CPU-cs423/raw/master/image/3.png)

Page Table Implememtation: Hierarchical strategy

![image](http://github.com/jiayuewang/Manually-implemented-CPU-cs423/raw/master/image/4.png)

TLB matching may different in different process/contents. One way is to flush every time change content; another way is to mark the content in TLB. Kernel TLB does not change, so there is speical part for kernel TLB.

![image](http://github.com/jiayuewang/Manually-implemented-CPU-cs423/raw/master/image/5.png)

Page fault: The physical memory page that the virtual memory the process refers to is not available in the memory, have to load from disk. Handle: if empty pages available, just load it. Else, replace the least resent used page in memory. Then restart the process.


Conclusion:

1. translate address. Use TLB to translate virtual address to physical address. If there is violation: 1. Try to access kernel address; 2. Most of the time the address is not available. If not in TLB, then look up page table. If page fault, load the page from disk to memory.

2. Then read/write if in cache, not the read memory.
An optimization


![image](http://github.com/jiayuewang/Manually-implemented-CPU-cs423/raw/master/image/6.png)

## File system
key abstractions: File, File name, Directory tree (folder)

file allocation: Platter, track, sector. Block: contain several sectors.

    allocation strategies: 1. Keep a free list; 2. Busy bit vector
    
    how to decide which block belong to which file: 1) FAT (file allocation table): glue the blocks together to file. And Directory table format: capture the hierarchy and starting blocks for the file.

![image](http://github.com/jiayuewang/Manually-implemented-CPU-cs423/raw/master/image/7.png)
2. Inode structure: uses double/triple/.. indirect pointer from 13bit.

![image](http://github.com/jiayuewang/Manually-implemented-CPU-cs423/raw/master/image/8.png)

Optimizations:
    
    Buffer Cache: in memory, for disk read. Dish reading are usually sequential. Pro: quick. Con: if the system crash before write back to dish, lost. Solution: call fsync or msync to flush.
    
    Journaling: instead of write to several parts in the dish, write to the journal in the disk first, then distribute to the other parts. Con: when reading, make sure check journal first.
    
    Direct memory access: cycle stealing. The direct memory controller steal cycles from memory directly from CPU. A hardware optimization.
    
![image](http://github.com/jiayuewang/Manually-implemented-CPU-cs423/raw/master/image/9.png)

![image](http://github.com/jiayuewang/Manually-implemented-CPU-cs423/raw/master/image/10.png)

Multithreaded Programming
    Asynchronous Computation. Example: web server <-> Browser. Keep CPU doing something instead of waiting resp
![image](http://github.com/jiayuewang/Manually-implemented-CPU-cs423/raw/master/image/11.png)

Thread exit cases: there is an exit call; the main thread exit: main thread exits, other threads are also terminated.

Thread Patterns:

Team model: A group of threads serve to request queue. “working pool”

Dispatcher model: A single dispatcher thread keep looking at the “work pool”

Pipeline model: one finishes pass to another.

Producer – consumer model. Use a ring buffer.

![image](http://github.com/jiayuewang/Manually-implemented-CPU-cs423/raw/master/image/12.png)

Mutual exclusion(mutex) vs synchronization.

Mutex: prevent two threads from accessing a resource at the same time

Synchronization: controlling where the threads are and the flow of execution.

Conditional waiting variable: B wait for a variable of A finish then continue doing sth


Networking

Interconnect layers: Application -> Transportation -> network ->link -> physical. (top -> bottom. -> uses the next.

NIC (network interface card) and the os. NIC also as DMA direct memory access. See before.

Link. Collision avoidance:
![image](http://github.com/jiayuewang/Manually-implemented-CPU-cs423/raw/master/image/13.png)

IP: internet protocol: net + ... The /# is the number of bits that are fixed. So we cannot use for ports.

![image](http://github.com/jiayuewang/Manually-implemented-CPU-cs423/raw/master/image/14Lan:

Local Area Network

and NAT (network address translation)

WAN:

Wide Area Network.png)


![image](http://github.com/jiayuewang/Manually-implemented-CPU-cs423/raw/master/image/15.png)


Lan:

Local Area Network

and NAT (network address translation)

WAN:

Wide Area Network

Internet routing

Routing table

Autonomous sytem

DNS: domain name service: translation ip->website address

![image](http://github.com/jiayuewang/Manually-implemented-CPU-cs423/raw/master/image/16.png)
Network Address Translation: private home network -> pusblic internet

![image](http://github.com/jiayuewang/Manually-implemented-CPU-cs423/raw/master/image/17.png)
Port: determine which process to receive a packet.

Destination IP: determine the last stop on an internet route

Mac: Determine which node should listen to data sent on a shared link.

TCP: transmission control protocol


![image](http://github.com/jiayuewang/Manually-implemented-CPU-cs423/raw/master/image/18.png)

UDP: user datagram protocol: some applications uses udp instead of tcp. UDP is simpler. If you have very reliable local network that does not need out of order protection, etc, of TCP, then use UDP. Who needs UDP? Streaming service, video, where timing is more important than quality.

![image](http://github.com/jiayuewang/Manually-implemented-CPU-cs423/raw/master/image/19.png)


