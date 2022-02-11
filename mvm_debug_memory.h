#if !defined(MVM_DEBUG_MEMORY_H)

// TODO(Marko): Author information etc

/*
    TODO(Marko): A list of things that would be nice to have:

                 - A configurable "max number of debug information" so that we  
                   eventually stop adding to the list, thereby preventing 
                   unfettered growth of the debug info
                 - A file-writing system that can write memory information to 
                   logs. 
                 - Pass __FILE__ and __LINE__ to TurnOnDebugInfo() and 
                   TurnOffDebugInfo(), and record all the places that they are 
                   called so that it's easy to find them and remove them when 
                   done. 
                 - Store the address of the variable that is allocated or 
                   freed. Possibly include old and new addresses for realloc?
*/

#define DEBUG_INFO_LIST_INITIAL_SIZE 32

#include <stdlib.h>

/* 
    NOTE(Marko): USAGE: #define DEBUG_MEMORY 
                        #include "mvm_debug_memory.h"

                        OR pass DDEBUG_MEMORY=1 as a compiler flag. 
*/

//
// NOTE(Marko): Debug string for internal use.
//

typedef struct mvm_debug_memory_string
{
    size_t Length;
    size_t MemoryAllocated;
    char *Contents;

} mvm_debug_memory_string;


mvm_debug_memory_string ConstStringToMVMDebugMemoryString(const char *String)
{
    mvm_debug_memory_string Result;
    Result.Length = strlen(String);
    Result.MemoryAllocated = 1;
    while(Result.MemoryAllocated <= Result.Length)
    {
        Result.MemoryAllocated *= 2;
    }

    Result.Contents = (char *)malloc(Result.MemoryAllocated);

    int CharIndex = 0;
    while(String[CharIndex])
    {
        Result.Contents[CharIndex] = String[CharIndex];
        CharIndex++;
    }

    // NOTE(Marko): Null terminate
    Result.Contents[CharIndex] = '\0';

    return Result;
}

typedef enum memory_operation_type
{
    MemoryOperationType_InitialAllocation,
    MemoryOperationType_ReAllocation,
    MemoryOperationType_Free,
    MemoryOperationType_Comment,
} memory_operation_type;


typedef struct mvm_debug_memory_info
{
    mvm_debug_memory_string Filename;
    memory_operation_type MemoryOperationType;
    int LineNumber;

} mvm_debug_memory_info;


typedef struct mvm_debug_memory_list
{
    size_t TurnOnCount;
    size_t DebugInfoCount;
    size_t MemoryAllocated;
    mvm_debug_memory_info *DebugInfoList;
} mvm_debug_memory_list;


// NOTE(Marko): Global Variable to hold the debug info. 
mvm_debug_memory_list *GlobalDebugInfoList = 0;


void TurnOnDebugInfo(void)
{
    
    if(!GlobalDebugInfoList)
    {
        // NOTE(Marko): If GlobalDebugInfo hasn't been initialized yet, 
        //              initialize it. 
        GlobalDebugInfoList = 
            (mvm_debug_memory_list *)malloc((sizeof *GlobalDebugInfoList));

        GlobalDebugInfoList->TurnOnCount = 1;
        GlobalDebugInfoList->DebugInfoCount = 0;
        GlobalDebugInfoList->MemoryAllocated = DEBUG_INFO_LIST_INITIAL_SIZE;

        GlobalDebugInfoList->DebugInfoList = 
            (mvm_debug_memory_info *)malloc(
                (sizeof GlobalDebugInfoList->DebugInfoList) * 
                GlobalDebugInfoList->MemoryAllocated);
    }
    else
    {
        GlobalDebugInfoList->TurnOnCount++;        
    }
}

void TurnOffDebugInfo(void)
{
    if(GlobalDebugInfoList)
    {
        if(GlobalDebugInfoList->TurnOnCount > 0)
        {
            GlobalDebugInfoList->TurnOnCount--;
        }
        else
        {
            printf("TurnOffDebugInfo() called without corresponding TurnOnDebugInfo()\n");
        }
    }
    else
    {
        printf("TurnOffDebugInfo() called before ever calling TurnOnDebugInfo()\n");
    }

}



void *MVMDebugMalloc(size_t MemorySize, 
                     const char *Filename, 
                     int LineNumber)
{
    void *Result = malloc(MemorySize);

    if(Result && GlobalDebugInfoList && (GlobalDebugInfoList->TurnOnCount > 0))
    {
        // TODO(Marko): This should probably be factored into a function where 
        //              you pass the Filename, LineNumber, MemoryAddress (i.e. 
        //              Result), and the MemoryOperationType. 
        // NOTE(Marko): Only commit information to the debug information list 
        //              if malloc() succeeded, and GlobalDebugInfoList has been 
        //              initialized, and the debug memory tool has been turned 
        //              on. 
        int DebugInfoIndex = GlobalDebugInfoList->DebugInfoCount;
        GlobalDebugInfoList->DebugInfoCount++;
        if(GlobalDebugInfoList->MemoryAllocated <= 
           GlobalDebugInfoList->DebugInfoCount)
        {
            // NOTE(Marko): Grow the debug info list if we've run out of 
            //              memory. 

            GlobalDebugInfoList->MemoryAllocated *= 2;
            GlobalDebugInfoList->DebugInfoList = 
                (mvm_debug_memory_info *)realloc(
                    GlobalDebugInfoList->DebugInfoList,
                    (sizeof GlobalDebugInfoList->DebugInfoList) * 
                    GlobalDebugInfoList->MemoryAllocated);
        }

        mvm_debug_memory_info *DebugInfo = 
            &GlobalDebugInfoList->DebugInfoList[DebugInfoIndex];
        DebugInfo->Filename = ConstStringToMVMDebugMemoryString(Filename);
        DebugInfo->MemoryOperationType = MemoryOperationType_InitialAllocation;
        DebugInfo->LineNumber = LineNumber;
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
    if(Buffer && GlobalDebugInfoList && (GlobalDebugInfoList->TurnOnCount > 0))
    {
        // NOTE(Marko): Only write to the debug info list if we are freeing 
        //              actual memory, if the GlobalDebugInfoList has been 
        //              initialized, and if we are currently in a Turned-On 
        //              state for the Debug Memory tool. 

        int DebugInfoIndex = GlobalDebugInfoList->DebugInfoCount;
        GlobalDebugInfoList->DebugInfoCount++;
        if(GlobalDebugInfoList->MemoryAllocated <= 
           GlobalDebugInfoList->DebugInfoCount)
        {
            // NOTE(Marko): Grow the debug info list if we've run out of 
            //              memory. 

            GlobalDebugInfoList->MemoryAllocated *= 2;
            GlobalDebugInfoList->DebugInfoList = 
                (mvm_debug_memory_info *)realloc(
                    GlobalDebugInfoList->DebugInfoList,
                    (sizeof GlobalDebugInfoList->DebugInfoList) * 
                    GlobalDebugInfoList->MemoryAllocated);
        }

        mvm_debug_memory_info *DebugInfo = 
            &GlobalDebugInfoList->DebugInfoList[DebugInfoIndex];
        DebugInfo->Filename = ConstStringToMVMDebugMemoryString(Filename);
        DebugInfo->MemoryOperationType = MemoryOperationType_Free;
        DebugInfo->LineNumber = LineNumber;      
    }

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