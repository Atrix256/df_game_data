#define DF_GAMEDATA_IMPLEMENTATION
#include "ShopData_C.h"

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
    ShopData_C_Database data;
    ShopData_C_InitDatabase(&data);

    if (!ShopData_C_LoadFromFile("packed/ShopData.bin", &data))
    {
        printf("Could not load packed/ShopData.bin\n");
        return 1;
    }

    ShopData_C_DestroyDatabase(&data);

    return 0;
}