# Lockless_Logger

*** This project is a work in progress and in no way represent the intended final outcome ***

The idea behind this utility is to reduce as much as possible the impact of logging on runtime.
Part of this reduction comes at the cost of having to parse and reorganize the messages in the log files
using a dedicated tool (yet to be implemented) as there is no guarantee on the order of logged messages.
 
The logger operates in the following way:
The logger provides several (number and size are user-defined) pre-allocated buffers which threads
can 'register' to and receive a private buffer.
In addition, a single, shared buffer is also pre-allocated (size is user-defined).
The number of buffers and their size is modifiable at runtime (not yet implemented).
 
The basic logic of the logger utility is that worker threads write messages in one of 3 ways that
will be described next, and an internal logger threads constantly iterates the existing buffers and
drains the data to the log file.
 
As all allocation are allocated at the initialization stage, no special treatment it needed for "out
of memory" cases.
 
The following writing levels exist:
Level 1 - Lockless writing:
  	Lockless writing is achieved by assigning each thread a private ring buffer.
  	A worker threads write to that buffer and the logger thread drains that buffer into
  	a log file.
 
In case the private ring buffer is full and not yet drained, or in case the worker thread has not
registered for a private buffer, we fall down to the following writing methods:
 
Level 2 - Shared buffer writing:
  	The worker thread will write it's data into a buffer that's shared across all threads.
  	This is done in a synchronized manner.

In case the private ring buffer is full and not yet drained AND the shared ring buffer is full and not
yet drained, or in case the worker thread has not registered for a private buffer, we fall down to
the following writing methods:
 
Level 3 - Direct write:
	This is the slowest form of writing - the worker thread directly write to the log file.

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
- Modify parameter passing mechanism to read all logger properties from a properties file
