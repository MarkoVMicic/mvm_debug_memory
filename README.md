# mvm_debug_memory
Work-in-progress tool for debugging manual memory management in C. 

This tool functions as a single-header-file drop-in that can be activated with a simple compiler define directive. 

It wraps memory management functions (e.g. `malloc()`) and records the file and line where they were called, to be displayed later.

You can specify where to turn on and turn off the memory debugging capability. These turn on and turn off calls are also recorded and can be nested. 

There is still a lot of work to be done to complete this tool. 
