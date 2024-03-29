#if !defined(MVM_DEBUG_MEMORY_H)

// TODO(Marko): Author information etc

/*
    TODO(Marko): A list of things that would be nice to have:
                 
                 - I think I need a linked list/growable array structure to 
                   store history for specific debug memory info to e.g. pair 
                   malloc() with realloc() and free(), and to pair turn-on 
                   with turn-off
                 - A file-writing system that can write memory information to 
                   logs. 
                 - Print should also tell you how many bytes of memory the 
                   GlobalDebugInfoList is using in total. (Currently just tells 
                   you how many list elements were allocated on the heap). 
                 - Store the address of the variable that is allocated or 
                   freed. Possibly include old and new addresses for realloc?
                 - Track reallocations! Make sure you trace the same variable 
                   as it gets repeatedly allocated
                 - A configurable "max number of debug information" so that we  
                   eventually stop adding to the list, thereby preventing 
                   unfettered growth of the debug info
                 - Explicit way to free the GlobalDebugInfoList -- perf reasons
                   (Granular API design)
                 - Heap Corruption detection? Like Page-aligned malloc. Perhaps 
                   this is too heavyweight and deserving of its status as a 
                   separate tool. 

*/

#define DEBUG_STRING_INITIAL_SIZE 32
#define DEBUG_INFO_LIST_INITIAL_SIZE 8
#define DEBUG_INFO_INTERNAL_ARRAY_INITIAL_SIZE 4
#define FREED_NOT_APPLICABLE -1

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


mvm_debug_memory_string *CreateEmptyMVMDebugString(void)
{
    mvm_debug_memory_string *Result = (mvm_debug_memory_string *)malloc(sizeof *Result);

    if(Result)
    {
        Result->Length = 0;
        Result->MemoryAllocated = DEBUG_STRING_INITIAL_SIZE;
        Result->Contents = (char *)malloc(Result->MemoryAllocated);
        if(!Result->Contents)
        {
            Result->MemoryAllocated = 0;
            Result->Contents = 0;
            printf("malloc failed while allocating memory for contents of memory debug string.\n\tAttempted to allocate %d bytes.\n\tFile: %s\n\tLine %d\n",
            Result->MemoryAllocated,
            __FILE__, 
            __LINE__);
        }
    }
    else
    {
        printf("malloc failed while allocating memory for mvm_debug_memory_string struct.\n\tAttempted to allocate %d bytes.\n\tFile: %s\n\tLine: %d\n", 
            (sizeof *Result), 
            __FILE__, 
            __LINE__);
    }

    return Result;
}


void ZeroInitializeEmptyMVMDebugString(mvm_debug_memory_string *MVMDebugString)
{
    MVMDebugString->Length = 0;
    MVMDebugString->MemoryAllocated = DEBUG_STRING_INITIAL_SIZE;
    MVMDebugString->Contents = (char *)malloc(MVMDebugString->MemoryAllocated);
    if(!MVMDebugString->Contents)
    {
        printf("malloc() failed when allocating memory for a string\n");
        MVMDebugString->MemoryAllocated = 0;
    }
}


void AppendConstStringToMVMDebugMemoryString(const char *Source,
                                             mvm_debug_memory_string *Dest)
{
    int SourceStringLength = strlen(Source);
    if(Dest->MemoryAllocated <= (Dest->Length + SourceStringLength))
    {
        while(Dest->MemoryAllocated <= (Dest->Length + SourceStringLength))
        {
            Dest->MemoryAllocated *= 2;
        }
        Dest->Contents = (char *)realloc(Dest->Contents, 
                                         Dest->MemoryAllocated);
    }

    int SourceStringIndex = 0;
    while(Source[SourceStringIndex])
    {
        Dest->Contents[Dest->Length++] = Source[SourceStringIndex++];
    }
    Dest->Contents[Dest->Length] = '\0';
}


void MVMAppendDebugString(mvm_debug_memory_string SourceMVMDebugString,
                          mvm_debug_memory_string AppendedMVMDebugString)
{
    size_t SourceMVMDebugStringCurrentIndex = SourceMVMDebugString.Length;

    SourceMVMDebugString.Length += AppendedMVMDebugString.Length;

    if(SourceMVMDebugString.MemoryAllocated <= SourceMVMDebugString.Length)
    {
        // NOTE(Marko): Grow the string if the memory allocated is too small to 
        //              fit the addition of the appended string
        while(SourceMVMDebugString.MemoryAllocated <= 
              SourceMVMDebugString.Length)
        {
            SourceMVMDebugString.MemoryAllocated *= 2;
        }
        SourceMVMDebugString.Contents = 
            realloc(SourceMVMDebugString.Contents, 
                    SourceMVMDebugString.MemoryAllocated);
    }

    for(int AppendedMVMDebugStringCurrentIndex = 0;
        AppendedMVMDebugStringCurrentIndex < AppendedMVMDebugString.Length;
        ++AppendedMVMDebugStringCurrentIndex, 
        ++SourceMVMDebugStringCurrentIndex)
    {
        SourceMVMDebugString.Contents[SourceMVMDebugStringCurrentIndex] = 
        AppendedMVMDebugString.Contents[AppendedMVMDebugStringCurrentIndex];
    }
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
    int DebugInfoOpCount;

    // NOTE(Marko): Growable array to store the number of bytes (de)allocated 
    //              using memory allocation ops. Negative values correspond 
    //              with freed amounts. 
    int ByteCountArrayCount;
    int ByteCountArrayAllocated;
    int *ByteCountArray;

    int FilenamesCount;
    int FilenamesAllocated;
    mvm_debug_memory_string *Filenames;

    int LineNumbersCount;
    int LineNumbersAllocated;
    int *LineNumbers;

    // TODO(Marko): Implement sort on the current address. This may make 
    //              finding addresses easier as we can use binary search. 
    // NOTE(Marko): - For malloc(), realloc(), stores the current address of 
    //                the pointer. 
    //              - For TurnOn and TurnOff, stores 0
    //              - When freed, stores -1
    void *CurrentAddress;
    // NOTE(Marko): PreviousAddress = 0 if not applicable.
    // NOTE(Marko): PreviousAddress used to check for usage after free. 
    void *PreviousAddress;

    void *InitialAddress;

    // NOTE(Marko): Indicator to show whether or not memory has been freed. -1 
    //              if it does not apply. 
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


mvm_debug_memory_info *
MVMSearchDebugInfoListByCurrentAddress(void *SearchedAddress)
{
    mvm_debug_memory_info *Result = 0;   
    if (GlobalDebugInfoList)
    {
        for(int DebugInfoUnitIndex = 0; 
            DebugInfoUnitIndex < GlobalDebugInfoList->DebugInfoUnitsCount; 
            DebugInfoUnitIndex++)
        {
            mvm_debug_memory_info *DebugInfoUnit = GlobalDebugInfoList->DebugInfoList + DebugInfoUnitIndex;
            if(DebugInfoUnit->CurrentAddress == SearchedAddress)
            {
                Result = DebugInfoUnit;
                break;
            }
        }
    }
    return(Result);
}


void MVMTurnOnDebugInfo(const char *Filename,
                        int LineNumber)
{
    
    if(!GlobalDebugInfoList)
    {
        // NOTE(Marko): If GlobalDebugInfo hasn't been initialized yet, 
        //              initialize it. 
        GlobalDebugInfoList = 
            (mvm_debug_memory_list *)malloc((sizeof *GlobalDebugInfoList));
        if(GlobalDebugInfoList)
        {
            *GlobalDebugInfoList = (mvm_debug_memory_list){0};
            GlobalDebugInfoList->TurnOnCount = 1;
            GlobalDebugInfoList->DebugInfoUnitsCount = 0;
            GlobalDebugInfoList->DebugInfoUnitsAllocated = DEBUG_INFO_LIST_INITIAL_SIZE;

            GlobalDebugInfoList->DebugInfoList = 
                (mvm_debug_memory_info *)malloc(
                    (sizeof *GlobalDebugInfoList->DebugInfoList) * 
                GlobalDebugInfoList->DebugInfoUnitsAllocated);
        }
        else
        {
            printf("malloc() failed when attempting to initially allocate the global mvm_debug_memory_list\n");
        }
    }
    else
    {
        GlobalDebugInfoList->TurnOnCount++;        
    }

    // NOTE(Marko): Add this turn on call to the GlobalDebugInfoList. 
    int DebugInfoIndex = GlobalDebugInfoList->DebugInfoUnitsCount;
    GlobalDebugInfoList->DebugInfoUnitsCount++;

    if(GlobalDebugInfoList->DebugInfoUnitsAllocated <= 
       GlobalDebugInfoList->DebugInfoUnitsCount)
    {
        // NOTE(Marko): Grow the size of the list since we've run out of 
        //              memory. 
        while(GlobalDebugInfoList->DebugInfoUnitsAllocated <= 
              GlobalDebugInfoList->DebugInfoUnitsCount)
        {
            GlobalDebugInfoList->DebugInfoUnitsAllocated *= 2;
        }

        GlobalDebugInfoList->DebugInfoList = 
            (mvm_debug_memory_info *)realloc(
                GlobalDebugInfoList->DebugInfoList,
                (sizeof GlobalDebugInfoList->DebugInfoList) * 
                GlobalDebugInfoList->DebugInfoUnitsAllocated);
    }

    mvm_debug_memory_info *DebugInfo = 
        GlobalDebugInfoList->DebugInfoList + DebugInfoIndex;

    *DebugInfo = (mvm_debug_memory_info){0};

    DebugInfo->DebugInfoOpCount = 1;

    // NOTE(Marko): This is not a memory allocation -- no bytes allocated. 
    DebugInfo->ByteCountArrayCount = 0;
    DebugInfo->ByteCountArrayAllocated = 0;
    DebugInfo->ByteCountArray = 0;

    // NOTE(Marko): MVMTurnOnDebugInfo() can only ever be associated 
    //              with one filename and one line number!
    DebugInfo->FilenamesAllocated = 1; 
    DebugInfo->Filenames = 
        (mvm_debug_memory_string *)malloc((sizeof *DebugInfo->Filenames) *
                                          DebugInfo->FilenamesAllocated);
    if(DebugInfo->Filenames)
    {        
        DebugInfo->FilenamesCount = 1;
        ZeroInitializeEmptyMVMDebugString(DebugInfo->Filenames);
        AppendConstStringToMVMDebugMemoryString(Filename,
                                                DebugInfo->Filenames);
    }
    else
    {
        printf("malloc() failed while allocating debug string structs\n");
        DebugInfo->FilenamesAllocated = 0;
        DebugInfo->FilenamesCount = 0;
    }

    DebugInfo->LineNumbersAllocated = 1; 
    DebugInfo->LineNumbers = 
        (int *)malloc((sizeof *DebugInfo->LineNumbers) * 
                      DebugInfo->LineNumbersAllocated);
    if(DebugInfo->LineNumbers)
    {
        DebugInfo->LineNumbers[0] = LineNumber;
        DebugInfo->LineNumbersCount = 1;
    }
    else
    {
        printf("malloc() failed while allocating array for line numbers\n");
        DebugInfo->LineNumbersAllocated = 0; 
        DebugInfo->LineNumbersCount = 0; 
    }

    DebugInfo->InitialAddress = 0;
    DebugInfo->CurrentAddress = 0;
    DebugInfo->PreviousAddress = 0;
    DebugInfo->Freed = FREED_NOT_APPLICABLE;

    // NOTE(Marko): TurnOn op only ever has one operation associated with it. 
    DebugInfo->MemoryOperationTypesAllocated = 1;
    DebugInfo->MemoryOperationTypes = 
        (memory_operation_type *)malloc(sizeof *DebugInfo->MemoryOperationTypes);
    if(DebugInfo->MemoryOperationTypes)
    {
        DebugInfo->MemoryOperationTypes[0] = MemoryOperationType_TurnOn;
        DebugInfo->MemoryOperationTypesCount = 1;
    }
    else
    {
        printf("malloc() failed while allocating array for memory op types\n");
        DebugInfo->MemoryOperationTypesAllocated = 0;
        DebugInfo->MemoryOperationTypesCount = 0;
    }

    DebugInfo->AddressesAllocated = 0;
    DebugInfo->AddressesCount = 0;
    DebugInfo->Addresses = 0;
}



void MVMTurnOffDebugInfo(const char *Filename,
                         int LineNumber)
{
    if(GlobalDebugInfoList)
    {
        if(GlobalDebugInfoList->TurnOnCount > 0)
        {
            GlobalDebugInfoList->TurnOnCount--;
            // NOTE(Marko): Add this turn off call to the debuginfolist. 
            int DebugInfoIndex = GlobalDebugInfoList->DebugInfoUnitsCount;
            GlobalDebugInfoList->DebugInfoUnitsCount++;

            if(GlobalDebugInfoList->DebugInfoUnitsAllocated <= 
               GlobalDebugInfoList->DebugInfoUnitsCount)
            {
                // NOTE(Marko): Grow the size of the list since we've run out 
                //              of memory. 
                while(GlobalDebugInfoList->DebugInfoUnitsAllocated <=
                      GlobalDebugInfoList->DebugInfoUnitsCount)
                {
                    GlobalDebugInfoList->DebugInfoUnitsAllocated *= 2;
                }
                GlobalDebugInfoList->DebugInfoList = 
                    (mvm_debug_memory_info *)realloc(
                        GlobalDebugInfoList->DebugInfoList,
                        (sizeof *GlobalDebugInfoList->DebugInfoList) * 
                        GlobalDebugInfoList->DebugInfoUnitsAllocated);
            }

            mvm_debug_memory_info *DebugInfo = 
                &GlobalDebugInfoList->DebugInfoList[DebugInfoIndex];

            *DebugInfo = (mvm_debug_memory_info){0};

            DebugInfo->DebugInfoOpCount = 1;

            // NOTE(Marko): This is not a memory allocation -- no bytes are 
            //              allocated.
            DebugInfo->ByteCountArrayCount = 0; 
            DebugInfo->ByteCountArrayAllocated = 0; 
            DebugInfo->ByteCountArray = 0; 

            // NOTE(Marko): MVMTurnOffDebugInfo() can only ever be associated 
            //              with one filename and one line number!
            DebugInfo->FilenamesAllocated = 1; 
            DebugInfo->Filenames = 
                (mvm_debug_memory_string *)malloc((sizeof *DebugInfo->Filenames) *
                                                  DebugInfo->FilenamesAllocated);
            if(DebugInfo->Filenames)
            {        
                DebugInfo->FilenamesCount = 1;
                ZeroInitializeEmptyMVMDebugString(DebugInfo->Filenames);
                AppendConstStringToMVMDebugMemoryString(Filename,
                                                        DebugInfo->Filenames);
            }
            else
            {
                printf("malloc() failed while allocating debug string structs\n");
                DebugInfo->FilenamesAllocated = 0;
                DebugInfo->FilenamesCount = 0;
            }

            DebugInfo->LineNumbersAllocated = 1; 
            DebugInfo->LineNumbers = 
                (int *)malloc((sizeof *DebugInfo->LineNumbers) * 
                              DebugInfo->LineNumbersAllocated);
            if(DebugInfo->LineNumbers)
            {
                DebugInfo->LineNumbers[0] = LineNumber;
                DebugInfo->LineNumbersCount = 1;
            }
            else
            {
                printf("malloc() failed while allocating array for line numbers\n");
                DebugInfo->LineNumbersAllocated = 0; 
                DebugInfo->LineNumbersCount = 0; 
            }

            DebugInfo->InitialAddress = 0;
            DebugInfo->CurrentAddress = 0;
            DebugInfo->PreviousAddress = 0;
            DebugInfo->Freed = FREED_NOT_APPLICABLE;

            // NOTE(Marko): TurnOff op only ever has one operation associated 
            //              with it. 
            DebugInfo->MemoryOperationTypesAllocated = 1;
            DebugInfo->MemoryOperationTypes = 
                (memory_operation_type *)malloc(sizeof *DebugInfo->MemoryOperationTypes);
            if(DebugInfo->MemoryOperationTypes)
            {
                DebugInfo->MemoryOperationTypes[0] = 
                    MemoryOperationType_TurnOff;
                DebugInfo->MemoryOperationTypesCount = 1;
            }
            else
            {
                printf("malloc() failed while allocating array for memory op types\n");
                DebugInfo->MemoryOperationTypesAllocated = 0;
                DebugInfo->MemoryOperationTypesCount = 0;
            }

            DebugInfo->AddressesAllocated = 0;
            DebugInfo->AddressesCount = 0;
            DebugInfo->Addresses = 0;
        }
        else
        {
            printf("TurnOffDebugInfo() called without corresponding TurnOnDebugInfo()\n");
            printf("TurnOffDebugInfo called in %s at line %d\n", 
                   Filename, 
                   LineNumber);
        }
    }
    else
    {
        printf("TurnOffDebugInfo() called before ever calling TurnOnDebugInfo()\n");
        printf("TurnOffDebugInfo called in %s at line %d\n", 
               Filename, 
               LineNumber);
    }
}



void *MVMDebugMalloc(size_t MemorySize, 
                     const char *Filename, 
                     int LineNumber)
{
    void *Result = 0;
    Result = malloc(MemorySize);

    // TODO(Marko): and else-if clauses that examine which thing in particular 
    //              failed: did malloc() fail, or was the GlobalDebugInfoList 
    //              not initialized, or was it not yet turned on? 
    if(Result && GlobalDebugInfoList && (GlobalDebugInfoList->TurnOnCount > 0))
    {
        // NOTE(Marko): Only commit information to the debug information list 
        //              if 
        //              1) malloc() succeeded 
        //              2) GlobalDebugInfoList has been initialized 
        //              3) the debug memory tool has been turned on. 

        int DebugInfoIndex = GlobalDebugInfoList->DebugInfoUnitsCount;
        GlobalDebugInfoList->DebugInfoUnitsCount++;

        if(GlobalDebugInfoList->DebugInfoUnitsAllocated <= 
           GlobalDebugInfoList->DebugInfoUnitsCount)
        {
            // NOTE(Marko): Grow the debug info list if we've run out of 
            //              memory. 
            while(GlobalDebugInfoList->DebugInfoUnitsAllocated <=
                  GlobalDebugInfoList->DebugInfoUnitsCount)      
            {
                GlobalDebugInfoList->DebugInfoUnitsAllocated *= 2;    
            }        
            GlobalDebugInfoList->DebugInfoList = 
                (mvm_debug_memory_info *)realloc(
                    GlobalDebugInfoList->DebugInfoList,
                    (sizeof *GlobalDebugInfoList->DebugInfoList) * 
                    GlobalDebugInfoList->DebugInfoUnitsAllocated);
        }

        mvm_debug_memory_info *DebugInfo = 
            &GlobalDebugInfoList->DebugInfoList[DebugInfoIndex];

        *DebugInfo = (mvm_debug_memory_info){0};

        // NOTE(Marko): malloc() is supposed to be the first op. 
        DebugInfo->DebugInfoOpCount = 1;

        // NOTE(Marko): malloc() is supposed to be associated with an 
        //              *initial* allocation. As such, we expect to be 
        //              allocating space for a series of reallocations and a 
        //              free at some point in the future. 

        //
        // NOTE(Marko): Initialize Byte Count Array to keep track of the 
        //              number of bytes allocated each time.
        //
        DebugInfo->ByteCountArrayAllocated = 
            DEBUG_INFO_INTERNAL_ARRAY_INITIAL_SIZE;
        DebugInfo->ByteCountArray = 
            (int *)malloc((sizeof *DebugInfo->ByteCountArray) * 
                          DebugInfo->ByteCountArrayCount);
        if(DebugInfo->ByteCountArray)
        {
            for(int i = 0; i < DebugInfo->ByteCountArrayAllocated; i++)
            {
                DebugInfo->ByteCountArray[i] = 0;
            }            
            DebugInfo->ByteCountArray[0] = (int)MemorySize;
            DebugInfo->ByteCountArrayCount = 1;
        }
        else
        {
            printf("malloc failed while allocating array for memory sizes\n");
            DebugInfo->ByteCountArrayAllocated = 0;
            DebugInfo->ByteCountArrayCount = 0;

        }


        //
        // NOTE(Marko): Initialize Filenames array
        //
        DebugInfo->FilenamesAllocated = DEBUG_INFO_INTERNAL_ARRAY_INITIAL_SIZE;
        DebugInfo->Filenames = 
            (mvm_debug_memory_string *)malloc((sizeof *DebugInfo->Filenames) * 
                                              DebugInfo->FilenamesAllocated);
        if(DebugInfo->Filenames)
        {
            for(int FilenameIndex = 0; 
                FilenameIndex < DebugInfo->FilenamesAllocated; 
                FilenameIndex++)
            {
                    mvm_debug_memory_string *CurrentFilename = 
                    DebugInfo->Filenames + FilenameIndex;
                ZeroInitializeEmptyMVMDebugString(CurrentFilename);
            }
            // NOTE(Marko): Fill in the first filename, since this is an 
            //              initial allocation. 
            AppendConstStringToMVMDebugMemoryString(Filename,
                                                    DebugInfo->Filenames + 0);
            DebugInfo->FilenamesCount = 1;
        }
        else
        {
            printf("malloc() failed while allocating debug string structs.\n");
            DebugInfo->FilenamesAllocated = 0;
            DebugInfo->FilenamesCount = 0;
        }

        //
        // NOTE(Marko): Initialize line numbers array
        //
        DebugInfo->LineNumbersAllocated = 
            DEBUG_INFO_INTERNAL_ARRAY_INITIAL_SIZE;
        DebugInfo->LineNumbers = 
            (int *)malloc((sizeof *DebugInfo->LineNumbers) * 
                           DebugInfo->LineNumbersAllocated);
        if(DebugInfo->LineNumbers)
        {
            for(int LineNumbersArrayIndex = 0;
                LineNumbersArrayIndex < DebugInfo->LineNumbersAllocated;
                LineNumbersArrayIndex++)
            {
                DebugInfo->LineNumbers[LineNumbersArrayIndex] = 0;
            }
            DebugInfo->LineNumbersCount = 1;
            DebugInfo->LineNumbers[0] = LineNumber;
        }
        else
        {
            printf("malloc() failed while allocating line number array.\n");
            DebugInfo->LineNumbersCount = 0;
            DebugInfo->LineNumbersAllocated = 0;
        }


        //
        // NOTE(Marko): Record the addresses
        //
        DebugInfo->CurrentAddress = Result;
        DebugInfo->InitialAddress = Result;
        DebugInfo->PreviousAddress = 0;
        DebugInfo->Freed = 0;


        //
        // NOTE(Marko): Initialize the Memory Operation Types array to 
        //              NotAssigned enum first
        //
        DebugInfo->MemoryOperationTypesAllocated = 
            DEBUG_INFO_INTERNAL_ARRAY_INITIAL_SIZE;
        DebugInfo->MemoryOperationTypesCount = 1;
        DebugInfo->MemoryOperationTypes = 
            (memory_operation_type *)malloc((sizeof *DebugInfo->MemoryOperationTypes)*DebugInfo->MemoryOperationTypesAllocated);
        if(DebugInfo->MemoryOperationTypes)
        {
            for(int MemOpIndex = 0;
                MemOpIndex < DebugInfo->MemoryOperationTypesAllocated;
                MemOpIndex++)
            {
                DebugInfo->MemoryOperationTypes[MemOpIndex] = 
                    MemoryOperationType_NotAssigned;
            }
            DebugInfo->MemoryOperationTypes[0] = 
                MemoryOperationType_InitialAllocation;
            DebugInfo->MemoryOperationTypesCount = 1;
        }
        else
        {
            printf("malloc() failed while allocating array for memory op types\n");
            DebugInfo->MemoryOperationTypesCount = 0;
            DebugInfo->MemoryOperationTypesAllocated = 0;
        }

        //
        // NOTE(Marko): Initialize array to hold memory addresses to 0 
        // 
        DebugInfo->AddressesAllocated =  DEBUG_INFO_INTERNAL_ARRAY_INITIAL_SIZE;
        DebugInfo->Addresses = 
            malloc((sizeof *DebugInfo->Addresses)*DebugInfo->AddressesAllocated); 
        if(DebugInfo->Addresses)
        {
            for(int AddressIndex = 0; 
                AddressIndex < DebugInfo->AddressesAllocated; 
                AddressIndex++)
            {
                DebugInfo->Addresses[AddressIndex] = 0;
            }
            DebugInfo->AddressesCount = 1;
            DebugInfo->Addresses[0] = Result;
        }
        else
        {
            printf("malloc failed while allocating array for memory addresses\n");
            DebugInfo->AddressesCount = 0;
            DebugInfo->AddressesAllocated = 0;

        }
    }
    return Result;

}



void *MVMDebugRealloc(void *Buffer, 
                      size_t MemorySize, 
                      const char *Filename, 
                      int LineNumber)
{
    void *Result = realloc(Buffer, MemorySize);

    if(Result && GlobalDebugInfoList && (GlobalDebugInfoList->TurnOnCount > 0))
    {
        // NOTE(Marko): Only commit information to the debug information list 
        //              if 
        //              1) realloc() succeeded 
        //              2) GlobalDebugInfoList has been initialized 
        //              3) the debug memory tool has been turned on. 
        mvm_debug_memory_info *DebugInfo = 
            MVMSearchDebugInfoListByCurrentAddress(Buffer);
        if(DebugInfo)
        {
            DebugInfo->DebugInfoOpCount++;

            //
            // NOTE(Marko): Add byte amount allocated to byte array
            //
            int ByteCountArrayIndex = DebugInfo->ByteCountArrayCount;
            DebugInfo->ByteCountArrayCount++;
            if(DebugInfo->ByteCountArrayAllocated <= 
               DebugInfo->ByteCountArrayCount)
            {
                while(DebugInfo->ByteCountArrayAllocated <= 
                      DebugInfo->ByteCountArrayCount)
                {
                    DebugInfo->ByteCountArrayAllocated *= 2;
                }
                DebugInfo->ByteCountArray = 
                    (int *)realloc(DebugInfo->ByteCountArray,
                                   (sizeof *DebugInfo->ByteCountArray) * 
                                   DebugInfo->ByteCountArrayAllocated);
            }
            DebugInfo->ByteCountArray[ByteCountArrayIndex] = (int)MemorySize;

            //
            // NOTE(Marko): Add filename to array
            //
            int FilenameIndex = DebugInfo->FilenamesCount; 
            DebugInfo->FilenamesCount++;
            if(DebugInfo->FilenamesAllocated < DebugInfo->FilenamesCount)
            {
                // NOTE(Marko): Increase size of filenames array if necessary
                while(DebugInfo->FilenamesAllocated <= 
                      DebugInfo->FilenamesCount)
                {
                    DebugInfo->FilenamesAllocated *= 2;
                }
                DebugInfo->Filenames = 
                    (mvm_debug_memory_string *)realloc(
                        DebugInfo->Filenames,
                        (sizeof *DebugInfo->Filenames) * 
                        DebugInfo->FilenamesAllocated);
                // NOTE(Marko): Zero-initialize the newly allocated empty 
                //              strings
                for(int NewAllocatedFilenameIndex = FilenameIndex;
                    NewAllocatedFilenameIndex < DebugInfo->FilenamesAllocated;
                    NewAllocatedFilenameIndex++)
                {
                    mvm_debug_memory_string *CurrentFilename = 
                        DebugInfo->Filenames + NewAllocatedFilenameIndex;
                    ZeroInitializeEmptyMVMDebugString(CurrentFilename);
                }
            }
            AppendConstStringToMVMDebugMemoryString(Filename,
                                                    DebugInfo->Filenames + 
                                                    FilenameIndex);

            //
            // NOTE(Marko): Add Line number to array
            //
            int LineNumberIndex = DebugInfo->LineNumbersCount;
            DebugInfo->LineNumbersCount++;
            if(DebugInfo->LineNumbersAllocated <= DebugInfo->LineNumbersCount)
            {
                // NOTE(Marko): Increase size of line numbers array if 
                //              necessary
                while(DebugInfo->LineNumbersAllocated <= 
                      DebugInfo->LineNumbersCount)
                {
                    DebugInfo->LineNumbersAllocated *= 2;
                }
                DebugInfo->LineNumbers = 
                    (int *)realloc(
                        DebugInfo->LineNumbers,
                        (sizeof *DebugInfo->LineNumbers) * 
                        DebugInfo->LineNumbersAllocated);
            }
            DebugInfo->LineNumbers[LineNumberIndex] = LineNumber;

            //
            // NOTE(Marko): Update the current and previous address
            //
            DebugInfo->PreviousAddress = DebugInfo->CurrentAddress;
            DebugInfo->CurrentAddress = Result;
            DebugInfo->Freed = 0;

            //
            // NOTE(Marko): Add memory operation type to array
            //
            int MemoryOperationTypeIndex = 
                DebugInfo->MemoryOperationTypesCount;
            DebugInfo->MemoryOperationTypesCount++;
            if(DebugInfo->MemoryOperationTypesAllocated <=
               DebugInfo->MemoryOperationTypesCount)
            {
                while(DebugInfo->MemoryOperationTypesAllocated <=
                      DebugInfo->MemoryOperationTypesCount)
                {
                    DebugInfo->MemoryOperationTypesAllocated *= 2;
                }
                DebugInfo->MemoryOperationTypes = 
                    (memory_operation_type *)realloc(
                        DebugInfo->MemoryOperationTypes,
                        (sizeof *DebugInfo->MemoryOperationTypes) * 
                        DebugInfo->MemoryOperationTypesAllocated);
            }
            DebugInfo->MemoryOperationTypes[MemoryOperationTypeIndex] =
                MemoryOperationType_ReAllocation;

            // 
            // NOTE(Marko): Add address to array
            //
            int AddressesIndex = DebugInfo->AddressesCount;
            DebugInfo->AddressesCount++;
            if(DebugInfo->AddressesAllocated <= 
                DebugInfo->AddressesCount)
            {
                // NOTE(Marko): Grow the addresses array if necessary
                while(DebugInfo->AddressesAllocated <=
                      DebugInfo->AddressesCount)
                {
                    DebugInfo->AddressesAllocated *= 2;
                }
                DebugInfo->Addresses = 
                    (void *)realloc(
                        DebugInfo->Addresses,
                        (sizeof *DebugInfo->Addresses) * 
                        DebugInfo->AddressesAllocated);
            }
            DebugInfo->Addresses[AddressesIndex] = 
                DebugInfo->CurrentAddress;
        }
        else
        {
            printf("Unable to find allocated memory located at %p in the debug info list.\n");
        }

    }

    return Result;

}

void MVMDebugFree(void *Buffer,
                  const char *Filename,
                  int LineNumber)
{
    // TODO(Marko): Separate this operation into two parts:
    //              1) Insert a new piece of information into the debug info 
    //                 list
    //              2) Search the Debug Info List for the memory operation 
    //                 that corresponds to the current address, and fill in 
    //                 the information there. 
    if(Buffer && GlobalDebugInfoList && (GlobalDebugInfoList->TurnOnCount > 0))
    {
        // NOTE(Marko): Only write to the debug info list if: 
        //              1) We are freeing actual memory, and not a null 
        //              pointer 
        //              2) The GlobalDebugInfoList has been initialized
        //              3) We are currently in a Turned-On state for the Debug 
        //                 Memory tool. 

        // NOTE(Marko): free() is supposed to free something that currently 
        //              exists. So we search via the current address.

        mvm_debug_memory_info *DebugInfo = 
            MVMSearchDebugInfoListByCurrentAddress(Buffer);

        if(DebugInfo)
        {
            DebugInfo->DebugInfoOpCount++;

            int ByteCountArrayIndex = DebugInfo->ByteCountArrayCount;
            DebugInfo->ByteCountArrayCount++;
            if(DebugInfo->ByteCountArrayAllocated <= 
               DebugInfo->ByteCountArrayCount)
            {
                while(DebugInfo->ByteCountArrayAllocated <= 
                      DebugInfo->ByteCountArrayCount)
                {
                    DebugInfo->ByteCountArrayAllocated *= 2;
                }
                DebugInfo->ByteCountArray = 
                    (int *)realloc(DebugInfo->ByteCountArray,
                                   (sizeof *DebugInfo->ByteCountArray) * 
                                   DebugInfo->ByteCountArrayAllocated);
            }
            // NOTE(Marko): Store negative number for freed memory
            DebugInfo->ByteCountArray[ByteCountArrayIndex] = 
                -DebugInfo->ByteCountArray[ByteCountArrayIndex-1];


            int FilenameIndex = DebugInfo->FilenamesCount; 
            DebugInfo->FilenamesCount++;
            if(DebugInfo->FilenamesAllocated < DebugInfo->FilenamesCount)
            {
                // NOTE(Marko): Increase size of filenames array if necessary
                while(DebugInfo->FilenamesAllocated <= 
                      DebugInfo->FilenamesCount)
                {
                    DebugInfo->FilenamesAllocated *= 2;
                }
                DebugInfo->Filenames = 
                    (mvm_debug_memory_string *)realloc(
                        DebugInfo->Filenames,
                        (sizeof *DebugInfo->Filenames) * 
                        DebugInfo->FilenamesAllocated);
                // NOTE(Marko): Zero-initialize the newly allocated empty 
                //              strings
                for(int NewAllocatedFilenameIndex = FilenameIndex;
                    NewAllocatedFilenameIndex < DebugInfo->FilenamesAllocated;
                    NewAllocatedFilenameIndex++)
                {
                    mvm_debug_memory_string *CurrentFilename = 
                        DebugInfo->Filenames + NewAllocatedFilenameIndex;
                    ZeroInitializeEmptyMVMDebugString(CurrentFilename);
                }
            }
            AppendConstStringToMVMDebugMemoryString(Filename,
                                                    DebugInfo->Filenames + 
                                                    FilenameIndex);

            int LineNumberIndex = DebugInfo->LineNumbersCount;
            DebugInfo->LineNumbersCount++;
            if(DebugInfo->LineNumbersAllocated <= DebugInfo->LineNumbersCount)
            {
                // NOTE(Marko): Increase size of line numbers array if 
                //              necessary
                while(DebugInfo->LineNumbersAllocated <= 
                      DebugInfo->LineNumbersCount)
                {
                    DebugInfo->LineNumbersAllocated *= 2;
                }
                DebugInfo->LineNumbers = 
                    (int *)realloc(
                        DebugInfo->LineNumbers,
                        (sizeof *DebugInfo->LineNumbers) * 
                        DebugInfo->LineNumbersAllocated);
            }
            DebugInfo->LineNumbers[LineNumberIndex] = LineNumber;

            DebugInfo->PreviousAddress = DebugInfo->CurrentAddress;
            // NOTE(Marko): Once freed, set current address to -1
            DebugInfo->CurrentAddress = (void *)-1;    
            DebugInfo->Freed = 1;

            int MemoryOperationTypeIndex = 
                DebugInfo->MemoryOperationTypesCount;
            DebugInfo->MemoryOperationTypesCount++;
            if(DebugInfo->MemoryOperationTypesAllocated <=
               DebugInfo->MemoryOperationTypesCount)
            {
                while(DebugInfo->MemoryOperationTypesAllocated <=
                      DebugInfo->MemoryOperationTypesCount)
                {
                    DebugInfo->MemoryOperationTypesAllocated *= 2;
                }
                DebugInfo->MemoryOperationTypes = 
                    (memory_operation_type *)realloc(
                        DebugInfo->MemoryOperationTypes,
                        (sizeof *DebugInfo->MemoryOperationTypes) * 
                        DebugInfo->MemoryOperationTypesAllocated);
            }
            DebugInfo->MemoryOperationTypes[MemoryOperationTypeIndex] =
                MemoryOperationType_Free;

            int AddressesIndex = DebugInfo->AddressesCount;
            DebugInfo->AddressesCount++;
            if(DebugInfo->AddressesAllocated <= 
                DebugInfo->AddressesCount)
            {
                // NOTE(Marko): Grow the addresses array if necessary
                while(DebugInfo->AddressesAllocated <=
                      DebugInfo->AddressesCount)
                {
                    DebugInfo->AddressesAllocated *= 2;
                }
                DebugInfo->Addresses = 
                    (void *)realloc(
                        DebugInfo->Addresses,
                        (sizeof *DebugInfo->Addresses) * 
                        DebugInfo->AddressesAllocated);
            }
            DebugInfo->Addresses[AddressesIndex] = 
                DebugInfo->CurrentAddress;

        }
        else
        {
            printf("Error while attempting to free address %p in file %s on line %d\n", Buffer, Filename, LineNumber);
            printf("Unable to find address at %p\n", Buffer);
        }
    }
    free(Buffer);
}


void MVMDebugMemoryComment(const char *MemoryComment,
                           const char *Filename,
                           int LineNumber)
{
    
}

void MVMDebugMemoryPrintAllocations(void)
{
    printf("*************************************************************\n");
    printf("*************************************************************\n");
    printf("Printing memory allocation information. \n\n");


    printf("Turn on - Turn Off calls (0 means debug is off now): %d\n", 
           GlobalDebugInfoList->TurnOnCount);
    printf("Debug Memory Operations Count: %d\n", 
           GlobalDebugInfoList->DebugInfoUnitsCount);
    printf("Debug Memory Operations Allocated: %d\n", 
           GlobalDebugInfoList->DebugInfoUnitsAllocated);

    unsigned int DebugMemoryInfoBytesAllocated = 0;
    unsigned int ProgramBytesAllocated = 0;

    printf("\n\n------------\n");

    for(int DebugInfoIndex = 0; 
        DebugInfoIndex < GlobalDebugInfoList->DebugInfoUnitsCount; 
        ++DebugInfoIndex)
    {
        printf("Debug Information Unit #%d:\n\n", DebugInfoIndex);

        mvm_debug_memory_info DebugInfo = 
            GlobalDebugInfoList->DebugInfoList[DebugInfoIndex];

        for(int MemoryOperationIndex = 0; 
            MemoryOperationIndex < DebugInfo.DebugInfoOpCount;
            MemoryOperationIndex++)
        {
            printf("\t---------\n");
            printf("\tMemory Operation #%d\n", MemoryOperationIndex);
            memory_operation_type MemoryOperation = 
                DebugInfo.MemoryOperationTypes[MemoryOperationIndex];

            printf("\t\tMemory Operation Type: ");
            switch(MemoryOperation)
            {
                case MemoryOperationType_InitialAllocation: 
                {
                    int BytesAllocated = 
                        DebugInfo.ByteCountArray[MemoryOperationIndex];
                    void *AddressAllocatedTo = 
                        DebugInfo.Addresses[MemoryOperationIndex];
                    mvm_debug_memory_string Filename = 
                        DebugInfo.Filenames[MemoryOperationIndex];
                    int LineNumber = 
                        DebugInfo.LineNumbers[MemoryOperationIndex];
                    printf("Initial Allocation.\n");
                    printf("\t\tAllocated %d bytes into address 0x%p\n",
                           BytesAllocated,
                           AddressAllocatedTo);
                    printf("\t\tin file %s\n", 
                           Filename.Contents);
                    printf("\t\ton line %d\n", 
                           LineNumber);

                } break;

                case MemoryOperationType_ReAllocation: 
                {
                    int PreviousByteCount = 
                        DebugInfo.ByteCountArray[MemoryOperationIndex-1];
                    int CurrentByteCount = 
                        DebugInfo.ByteCountArray[MemoryOperationIndex];
                    int ByteAllocationDifference = 
                        CurrentByteCount - PreviousByteCount;
                    char ByteDifferenceSign = 
                        (ByteAllocationDifference >= 0) ? '+' : '-';
                    void *InitialAddress = DebugInfo.InitialAddress;
                    void *PreviousAddress = 
                        DebugInfo.Addresses[MemoryOperationIndex-1];
                    void *CurrentAddress = 
                        DebugInfo.Addresses[MemoryOperationIndex];
                    mvm_debug_memory_string Filename = 
                        DebugInfo.Filenames[MemoryOperationIndex];
                    int LineNumber = 
                        DebugInfo.LineNumbers[MemoryOperationIndex];

                    printf("Reallocation\n");
                    printf("\t\tOld allocation: %d bytes from address 0x%p\n",
                           PreviousByteCount,
                           PreviousAddress);
                    printf("\t\tNew allocation: %d bytes into address 0x%p\n",
                           CurrentByteCount,
                           CurrentAddress);
                    printf("\t\tAllocation changed by %c%d bytes\n",
                           ByteDifferenceSign, 
                           ByteAllocationDifference);
                    printf("\t\tInitial address: 0x%p\n",
                           InitialAddress);
                    printf("\t\tin file %s\n", 
                           Filename.Contents);
                    printf("\t\ton line %d\n", 
                           LineNumber);

                } break;

                case MemoryOperationType_Free: 
                {
                    int BytesFreed = 
                        -DebugInfo.ByteCountArray[MemoryOperationIndex];
                    void *AddressFreed = 
                        DebugInfo.PreviousAddress;
                    void *InitialAddress = 
                        DebugInfo.InitialAddress;
                    mvm_debug_memory_string Filename = 
                        DebugInfo.Filenames[MemoryOperationIndex];
                    int LineNumber = 
                        DebugInfo.LineNumbers[MemoryOperationIndex];

                    printf("Free\n");
                    printf("\t\tFreed %d bytes from address 0x%p\n",
                           BytesFreed,
                           AddressFreed);
                    printf("\t\tMemory Initially allocated into address 0x%p\n", 
                           InitialAddress);                        
                    printf("\t\tin file %s\n", 
                           Filename.Contents);
                    printf("\t\ton line %d\n", 
                           LineNumber);
                } break;

                case MemoryOperationType_Comment: 
                {

                } break;


                case MemoryOperationType_TurnOn: 
                {
                    mvm_debug_memory_string Filename = 
                        DebugInfo.Filenames[MemoryOperationIndex];
                    int LineNumber = 
                        DebugInfo.LineNumbers[MemoryOperationIndex];
                    printf("Turn On Debug Tool\n");
                    printf("\t\tin file %s\n", 
                           Filename.Contents);
                    printf("\t\ton line %d\n", 
                           LineNumber);
                } break;
                
                case MemoryOperationType_TurnOff: 
                {
                    mvm_debug_memory_string Filename = 
                        DebugInfo.Filenames[MemoryOperationIndex];
                    int LineNumber = 
                        DebugInfo.LineNumbers[MemoryOperationIndex];
                    printf("Turn Off Debug Tool\n");
                    printf("\t\tin file %s\n", 
                           Filename.Contents);
                    printf("\t\ton line %d\n", 
                           LineNumber);
                } break;
            }
        }
        printf("--------------------------------------------\n\n");
    }      
    printf("\n\n");
}


// NOTE(Marko): These #define replacements need to come after the function 
//              declarations to avoid infinite recursion problems. 
#if defined(MVM_DEBUG_MEMORY)


    #define malloc(n) MVMDebugMalloc(n, __FILE__, __LINE__)
    #define realloc(m, n) MVMDebugRealloc(m, n, __FILE__, __LINE__)
    #define free(n) MVMDebugFree(n, __FILE__, __LINE__)

    #define MVMTurnOnDebugInfo() MVMTurnOnDebugInfo(__FILE__, __LINE__)
    #define MVMTurnOffDebugInfo() MVMTurnOffDebugInfo(__FILE__, __LINE__)
    #define MVMDebugMemoryComment(m) MVMDebugMemoryComment(m, __FILE__, __LINE__)

#else
    // NOTE(Marko): This allows you to write memory comments and memory print 
    //              commands that can be preprocessed out easily in a release 
    //              build
    #define MVMDebugMemoryComment(m) 
    #define MVMDebugMemoryPrintAllocations() 
    #define InitializeDebugInfo()
    #define FreeDebugInfo()
    #define MVMTurnOnDebugInfo() 
    #define MVMTurnOffDebugInfo() 

#endif

#define MVM_DEBUG_MEMORY_H
#endif