# CpuTopology

Module comment for CpuTopology.h / CpuTopology.cpp.

## cpu_topology

Hardware facts the program tunes itself to. Kept apart from LlamaClient so it can be tested
without linking llama.cpp.

## physical_cores

The number of physical cores, not hardware threads: llama.cpp computes with one thread per core
fastest, and two threads sharing a core through hyper-threading compete for the same arithmetic
units. Measured with Qwen2.5-Coder-3B on an i7-7820HQ (4 cores, 8 threads): 8 threads were 30 to
35 percent slower than 4, both for prompt processing and for generating the answer.

## sibling-lists

Every logical CPU lists the hardware threads of its core; the threads of one core share the
same list, so the number of distinct lists is the number of cores - across several sockets too,
since the lists name global CPU numbers. Offline CPUs have no topology directory and are not
counted; entries like `cpufreq` that are no CPU have none either.

## fallback

Falls back to the hardware threads when the platform tells nothing, and never exceeds them, so
a misread topology can cost speed but never yield zero threads.
