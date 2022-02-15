#include "mvm_debug_memory.h"
// #include <stdlib.h>


int main(int argc, char **argv)
{
    TurnOnDebugInfo();

    int *InformationArray = (int *)malloc((sizeof *InformationArray) * 32);

    free(InformationArray);

    TurnOffDebugInfo();

    int *NoInformationArray = (int *)malloc((sizeof *InformationArray) * 64);

    free(NoInformationArray);

    MVMDebugMemoryPrintAllocations();


    return(0);   
}

