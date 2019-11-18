# Lockless_Logger

*** This project is a work in progress and in no way represent the intended final outcome ***

A lockless logger utility which works on 3 levels:
Level 1 - Lockless writing:
 	Lockless writing is achieved by assigning each thread a private ring buffer.
 	A worker threads write to that buffer and the logger thread drains that buffer into
 	a log file.
Level 2 - Shared buffer writing:
  	In case the private ring buffer is full and not yet drained, a worker thread will
  	fall down to writing to a shared buffer (which is shared across all workers).
  	This is done in a synchronized manner.
Level 3 - In case the shared buffer is also full and not yet
  	drained, a worker thread will fall to the lowest (and slowest) form of writing - direct
 	file write.
