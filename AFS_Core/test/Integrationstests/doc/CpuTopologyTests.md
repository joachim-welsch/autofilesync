# CpuTopologyTests

Module comment for CpuTopologyTests.cpp.

## PhysicalCoresBetweenOneAndHardwareThreads

The exact number depends on the machine running the test, so only the bounds every machine
satisfies are checked: llama.cpp gets at least one thread and never more than there are
hardware threads.

## HyperThreadingHalvesAtMost

Hyper-threading runs at most two threads per core on the processors the program supports;
fewer cores than half the hardware threads means the topology was misread and several cores
were merged into one - llama.cpp would then leave cores idle.
