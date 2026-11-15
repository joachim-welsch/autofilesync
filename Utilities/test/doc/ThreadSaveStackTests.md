# ThreadSaveStackTests

Module comment for ThreadSaveStackTests.cpp.

## ConcurrentPushAndPopLoseAndDuplicateNothing

Four threads push distinct numbers while four others pop. A consumer may only stop after a pop
on an empty stack that began when all producers had already finished - last_round is read before
the pop for that reason; read afterwards, a value pushed in between could stay behind. Count and
sum of the popped values together show that every number came out exactly once: a lost value
lowers the count, a duplicated one raises it.
