#if !defined(MVM_DEBUG_MEMORY_H)

#include <stdlib.h>

/* 
    NOTE(Marko): USAGE: #define DEBUG_MEMORY and then #include "mvm_debug_memory.h"
*/

enum memory_operation_type
{
    MemoryOperationType_InitialAllocation,
    MemoryOperationType_ReAllocation,
    MemoryOperationType_Free,
    MemoryOperationType_Comment,
};


// TODO(Marko): Where are we going to store the debug memory information? 
//              Presumably somewhere global? How does that work? Maybe write to 
//              file for each allocation? hmmmm
// NOTE(Marko): Using files would enable the usage of other tooling. Less work 
//              for me? 
typedef struct mvm_debug_memory_info
{
    memory_operation_type MemoryOperationType;
    int Lines;
    // TODO(Marko): Custom string? C strings really suck
    char *Filenames;

} mvm_debug_memory_info;

mvm_debug_memory_info *GlobalDebugInfo = 0;
int GlobalDebugInfoCount = 0;
int GlobalDebugOnCount = 0;


void TurnOnDebugInfo(void)
{
    // initialize mvm_debug_memory_info on the heap
    if(!GlobalDebugInfo)
    {
       //actually initialize 
    }
    GlobalDebugOn++;
}

void TurnOffDebugInfo(void)
{

}



void *MVMDebugMalloc(size_t MemorySize, 
                     const char *Filename, 
                     int LineNumber)
{
    void *Result = malloc(MemorySize);

    if(GlobalDebugOn)
    {
        printf("File: %s\n", Filename);
        printf("Line: %d\n", LineNumber);        
    }

    return Result;

}



void *MVMDebugRealloc(void *Buffer, 
                      size_t MemorySize, 
                      const char *Filename, 
                      int LineNumber)
{
    void *Result = realloc(Buffer, MemorySize);

    return Result;

}

void MVMDebugFree(void *Buffer,
                  const char *Filename,
                  int LineNumber)
{
    printf("File: %s", Filename);
    printf("Line: %d\n", LineNumber);
    free(Buffer);
}


void MVMDebugMemoryComment(char *MemoryComment)
{
    
}

void MVMDebugMemoryPrintAllocations(void)
{

}


// NOTE(Marko): These #define replacements need to come after the function 
//              declarations to avoid infinite recursion problems. 
#if defined(MVM_DEBUG_MEMORY)


    #define malloc(n) MVMDebugMalloc(n, __FILE__, __LINE__)
    #define realloc(m, n) MVMDebugRealloc(m, n, __FILE__, __LINE__)
    #define free(n) MVMDebugFree(n, __FILE__, __LINE__)

    #define TurnOn() TurnOnDebugInfo(__FILE__, __LINE__)

#else
    // NOTE(Marko): This allows you to write memory comments and memory print 
    //              commands that can be preprocessed out easily in a release 
    //              build
    #define MVMDebugMemoryComment(m) 
    #define MVMDebugMemoryPrintAllocations() 
    #define InitializeDebugInfo()
    #define FreeDebugInfo()


#endif


#define MVM_DEBUG_MEMORY_H
#endif