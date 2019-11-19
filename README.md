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

For project documentation visit:
https://baraksason.github.io/Lockless_Logger/

TODO List:
- Add a feature that enables to modify number of buffers at runtime
- Add a feature that enables to modify size of buffers at runtime
- Create log file naiming mechanism
- Create rotating log file mechanism
- Create a parsing utility for the log files
- Create unit tests for logger API (currently the logger component is only system tested)
- Add more status codes and return specific code per failure (not just success / failure)
