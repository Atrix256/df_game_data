
#include "Data.h"

#define VERIFY(x) if (!Verify(x, #x)) return 1;

bool Verify(bool value, const char* cond)
{
    if (value)
        return true;

    printf("Condition failed: %s\n", cond);

    return false;
}

int main(int argc, char** argv)
{
    Data data;
    if (!data.LoadFromFile("packed/Data.bin"))
    {
        printf("Could not load packed/Data.bin");
        return 1;
    }

    printf("All checks passed!");
    return 0;
}
