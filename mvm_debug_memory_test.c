#include "mvm_debug_memory.h"
// #include <stdlib.h>


int main(int argc, char **argv)
{
    MVMTurnOnDebugInfo();

    int *InformationArray = (int *)malloc((sizeof *InformationArray) * 32);

    InformationArray = (int *)realloc(InformationArray, 
                                      (sizeof *InformationArray) * 64);

    free(InformationArray);

    MVMTurnOffDebugInfo();

    int *NoInformationArray = (int *)malloc((sizeof *InformationArray) * 64);

    free(NoInformationArray);

    MVMDebugMemoryPrintAllocations();


    return(0);   
}

