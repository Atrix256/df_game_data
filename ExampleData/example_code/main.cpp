#include <stdio.h>

#include "data/data.h"

int main(int argc, char** argv)
{
    dfgd data;
    if (!data.LoadFromFile("data/data.bin"))
    {
        printf("Could not load data!\n");
        return 1;
    }
    int ijkl = 0;

    // TODO: load data
    return 0;
}
