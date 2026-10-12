#define DF_GAMEDATA_IMPLEMENTATION
#include "Data_C.h"
#include "Data_No_Obfuscation_C.h"

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
    return 0;
}
