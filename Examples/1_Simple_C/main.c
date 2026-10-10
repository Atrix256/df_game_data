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

    VERIFY(data.table_Item_count == 3);
    ShopData_C_Item* items = (ShopData_C_Item*)data.table_Item;

    // test index 0
    {
        ShopData_C_Item* item = &items[0];
        VERIFY(!strcmp((const char*)item->name, "Gladius"));
        VERIFY(item->sellCost == 2);
        VERIFY(item->buyCost == 20);
    }

    // test index 1
    {
        ShopData_C_Item* item = &items[1];
        VERIFY(!strcmp((const char*)item->name, "Long Sword"));
        VERIFY(item->sellCost == 5);
        VERIFY(item->buyCost == 50);
    }

    // test index 2
    {
        ShopData_C_Item* item = &items[2];
        VERIFY(!strcmp((const char*)item->name, "Rusty Dagger"));
        VERIFY(item->sellCost == 1);
        VERIFY(item->buyCost == 10);
    }

    // test getting by name
    {
        size_t index = ShopData_C_StringIndex((const char**)data.table_Item_names, data.table_Item_count, "Rusty Dagger");
        VERIFY(index < data.table_Item_count);
        ShopData_C_Item* item = &items[index];
        VERIFY(!strcmp((const char*)item->name, "Rusty Dagger"));
    }

    printf("All checks passed!\n");

    ShopData_C_DestroyDatabase(&data);

    return 0;
}
