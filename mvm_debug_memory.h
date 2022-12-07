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
#define DEBUG_INFO_INTERNAL_ARRAY_INITIAL_SIZE 8

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
    MemoryOperationType_NotAssigned = 0,

    MemoryOperationType_InitialAllocation,
    MemoryOperationType_ReAllocation,
    MemoryOperationType_Free,
    MemoryOperationType_Comment,
    MemoryOperationType_TurnOn,
    MemoryOperationType_TurnOff,


} memory_operation_type;


typedef struct mvm_debug_memory_info
{
    mvm_debug_memory_string Filename;
    int LineNumber;

    // TODO(Marko): Implement sort on the current address. This will make 
    //              finding the allocation easier.
    // NOTE(Marko): - For malloc(), realloc(), stores the current address of 
    //                the pointer. 
    //              - For TurnOn and TurnOff, stores 0
    //              - When freed, stores -1
    void *CurrentAddress;
    // NOTE(Marko): PreviousAddress = 0 if not applicable.
    // NOTE(Marko): PreviousAddress used to check for usage after free. 
    void *PreviousAddress;

    void *InitialAddress;
    int Freed;

    int MemoryOperationTypesAllocated;
    int MemoryOperationTypesCount;
    memory_operation_type *MemoryOperationTypes;

    // NOTE(Marko): Store current address of allocation for easier search. 
    //              This is to ensure that reallocations are easier to find
    int AddressesAllocated;
    int AddressesCount;
    void **Addresses; 

} mvm_debug_memory_info;


typedef struct mvm_debug_memory_list
{
    size_t TurnOnCount;
    size_t DebugInfoUnitsCount;
    size_t DebugInfoUnitsAllocated;
    mvm_debug_memory_info *DebugInfoList;
    
} mvm_debug_memory_list;


// NOTE(Marko): Global Variable to hold the debug info. 
mvm_debug_memory_list *GlobalDebugInfoList = 0;


void TurnOnDebugInfo(const char *Filename,
                     int LineNumber)
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

    // NOTE(Marko): Add this turn on call to the debuginfolist. 
    int DebugInfoIndex = GlobalDebugInfoList->DebugInfoCount;
    GlobalDebugInfoList->DebugInfoCount++;

    if(GlobalDebugInfoList->MemoryAllocated <= 
       GlobalDebugInfoList->DebugInfoCount)
    {
        // NOTE(Marko): Grow the size of the list since we've run out of 
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
    DebugInfo->MemoryOperationType = MemoryOperationType_TurnOn;
    DebugInfo->LineNumber = LineNumber;
}

void TurnOffDebugInfo(void)
void TurnOffDebugInfo(const char *Filename,
                      int LineNumber)
{
    if(GlobalDebugInfoList)
    {
        if(GlobalDebugInfoList->TurnOnCount > 0)
        {
            GlobalDebugInfoList->TurnOnCount--;
            // NOTE(Marko): Add this turn off call to the debuginfolist. 
            int DebugInfoIndex = GlobalDebugInfoList->DebugInfoCount;
            GlobalDebugInfoList->DebugInfoCount++;

            if(GlobalDebugInfoList->MemoryAllocated <= 
               GlobalDebugInfoList->DebugInfoCount)
            {
                // NOTE(Marko): Grow the size of the list since we've run out 
                //              of memory. 
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
            DebugInfo->MemoryOperationType = MemoryOperationType_TurnOff;
            DebugInfo->LineNumber = LineNumber;
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
    void *Result = 0;
    Result = malloc(MemorySize);

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

    printf("Turn on - Turn Off calls (0 means debug is off now): %d\n", 
           GlobalDebugInfoList->TurnOnCount);
    printf("Debug Memory Operations Count: %d\n", 
           GlobalDebugInfoList->DebugInfoCount);

    // TODO(Marko): Compute total amount of memory used for debugging purposes 
    //              in bytes. 
    printf("Debug Info List Allocated Size: %d\n",
           GlobalDebugInfoList->MemoryAllocated);

    printf("------------\n");
    for(int DebugInfoIndex = 0; 
        DebugInfoIndex < GlobalDebugInfoList->DebugInfoCount; 
        ++DebugInfoIndex)
    {
        printf("Allocation %d:\n", DebugInfoIndex);

        mvm_debug_memory_info DebugInfo = 
            GlobalDebugInfoList->DebugInfoList[DebugInfoIndex];
        
        char MemoryOperationTypeString[32];
        switch(DebugInfo.MemoryOperationType)
        {
            case MemoryOperationType_InitialAllocation:
            {
                strcpy(MemoryOperationTypeString, "malloc");

            } break;
            case MemoryOperationType_ReAllocation:
            {
                strcpy(MemoryOperationTypeString, "realloc");

            } break;
            case MemoryOperationType_Free:
            {
                strcpy(MemoryOperationTypeString, "free");

            } break;
            case MemoryOperationType_TurnOn:
            {
                strcpy(MemoryOperationTypeString, "TurnOn");
            } break;
            case MemoryOperationType_TurnOff:
            {
                strcpy(MemoryOperationTypeString, "TurnOff");
            } break;
            case MemoryOperationType_Comment:
            {
                strcpy(MemoryOperationTypeString, "\0");
            } break;
        }
        printf("\t%s called\n", MemoryOperationTypeString);
        printf("\tin file %s\n", DebugInfo.Filename.Contents);
        printf("\ton line %d\n", DebugInfo.LineNumber);
        printf("------------\n");
    }
    printf("\n\n");
}


// NOTE(Marko): These #define replacements need to come after the function 
//              declarations to avoid infinite recursion problems. 
#if defined(MVM_DEBUG_MEMORY)


    #define malloc(n) MVMDebugMalloc(n, __FILE__, __LINE__)
    #define realloc(m, n) MVMDebugRealloc(m, n, __FILE__, __LINE__)
    #define free(n) MVMDebugFree(n, __FILE__, __LINE__)

    #define TurnOnDebugInfo() TurnOnDebugInfo(__FILE__, __LINE__)
    #define TurnOffDebugInfo() TurnOffDebugInfo(__FILE__, __LINE__)

#else
    // NOTE(Marko): This allows you to write memory comments and memory print 
    //              commands that can be preprocessed out easily in a release 
    //              build
    #define MVMDebugMemoryComment(m) 
    #define MVMDebugMemoryPrintAllocations() 
    #define InitializeDebugInfo()
    #define FreeDebugInfo()
    #define TurnOnDebugInfo() 
    #define TurnOffDebugInfo() 

#endif


#define MVM_DEBUG_MEMORY_H
#endif