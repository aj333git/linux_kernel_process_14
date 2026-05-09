Advanced Linux Kernel SLAB Allocator

Advanced Linux kernel module demonstrating:

Custom SLAB cache creation
Shrinker integration
Refcounted kernel objects
Per-CPU statistics
Procfs runtime monitoring
Spinlock synchronization
SLAB poisoning
Red-zone debugging
Cacheline alignment
Aging-based reclaim logic

This project demonstrates how real Linux kernel subsystems efficiently manage frequently allocated small objects.

| Feature              | Description                       |
| -------------------- | --------------------------------- |
| Custom SLAB Cache    | Fast object allocation            |
| Constructor (`ctor`) | Initializes objects automatically |
| Shrinker API         | Reclaims memory under pressure    |
| Refcounting          | Safe object lifecycle management  |
| Per-CPU Stats        | Reduces contention                |
| Procfs Interface     | Runtime observability             |
| Spinlocks            | Synchronization protection        |
| SLAB Poisoning       | Detects use-after-free            |
| Red Zones            | Detects buffer overflows          |
| Cache Alignment      | Improves CPU cache locality       |



Kernel Concepts Demonstrated
Linux SLAB allocator
Kernel memory management
Concurrency and synchronization
Shrinker subsystem
Per-CPU data
Object-oriented patterns in C
Runtime memory reclaim
Kernel debugging infrastructure

SLAB Allocator Overview

The Linux SLAB allocator maintains caches of pre-initialized objects.

Benefits:

Faster allocations
Reduced fragmentation
Better cache locality
Lower allocation overhead
Efficient object reuse


SLAB Allocator Overview

The Linux SLAB allocator maintains caches of pre-initialized objects.

Benefits:

Faster allocations
Reduced fragmentation
Better cache locality
Lower allocation overhead
Efficient object reuse


| API                    | Purpose                |
| ---------------------- | ---------------------- |
| `kmem_cache_create()`  | Create slab cache      |
| `kmem_cache_alloc()`   | Allocate object        |
| `kmem_cache_free()`    | Free object            |
| `kmem_cache_destroy()` | Destroy cache          |
| `register_shrinker()`  | Register reclaim logic |
| `proc_create()`        | Create procfs entry    |



Build Module
make

Sign Kernel Module (Secure Boot Systems)
sudo /usr/src/linux-headers-$(uname -r)/scripts/sign-file sha256 \
~/kernel_keys/MOK.key \
~/kernel_keys/MOK.crt \
custom_slab_advanced.ko

Load Module
sudo insmod custom_slab_advanced.ko

View Kernel Logs
dmesg -w
or
sudo journalctl -kf

Verify Module Loaded

lsmod | grep custom_slab


Inspect SLAB Cache
sudo cat /proc/slabinfo | grep my_object_cache
or 
sudo slabtop

Trigger Memory Pressure

This forces reclaim activity and shrinker execution.


stress-ng --vm 4 --vm-bytes 80% --timeout 30s

Enable Function Tracing

sudo sh -c 'echo function > /sys/kernel/debug/tracing/current_tracer'

or 

echo function | sudo tee /sys/kernel/debug/tracing/current_tracer

Verify Active Tracer

sudo cat /sys/kernel/debug/tracing/current_tracer

Remove Module

sudo rmmod custom_slab_advanced


Check Recent Kernel Logs

After module insertion:

dmesg | tail -n 20

After module removal:

dmesg | tail -n 20


kmem_cache_create()
        ↓
allocate_object()
        ↓
linked list tracking
        ↓
runtime usage
        ↓
shrinker reclaim
        ↓
kmem_cache_free()
        ↓
kmem_cache_destroy()

| Subsystem          | Cached Structure          |
| ------------------ | ------------------------- |
| Process Management | `task_struct`             |
| VFS                | `inode`, `dentry`, `file` |
| Networking         | `sk_buff`                 |
| Memory Management  | `vm_area_struct`          |


Best Practices Demonstrated
Use constructors for initialization
Prefer strscpy() over strcpy()
Use cacheline alignment
Always destroy slab caches
Protect shared structures with spinlocks
Reclaim unused memory proactively
Use refcounting for ownership safety




Advanced Topics Covered
Custom allocator design
Kernel synchronization
Memory reclaim internals
Per-CPU optimization
Kernel object lifecycle
Debugging memory corruption
Kernel observability

Future Improvements

Potential senior-level upgrades:

Lockless hash tables
RCU-based reclamation
memcg awareness
eBPF observability
Tracepoints integration
NUMA-aware allocation
Slab poisoning verification
Kernel selftests
Lock-free freelists


References
Linux Kernel Documentation
Linux Device Drivers
Understanding the Linux Kernel
/proc/slabinfo
slabtop
Linux Memory Management Documentation


