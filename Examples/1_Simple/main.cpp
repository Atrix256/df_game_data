
#include "ShopData.h"

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
    ShopData data;
    if (!data.LoadFromFile("packed/ShopData.bin"))
    {
        printf("Could not load packed/ShopData.bin\n");
        return 1;
    }

    VERIFY(data.GetItemCount() == 3);

    // test index 0
    {
        ShopData::ItemRecord itemRecord = data.GetItem(0);
        VERIFY(itemRecord.Valid());
        VERIFY(!strcmp(itemRecord.Get().name.ptr, "Gladius"));
        VERIFY(itemRecord.Get().sellCost == 2);
        VERIFY(itemRecord.Get().buyCost == 20);
    }

    // test index 1
    {
        ShopData::ItemRecord itemRecord = data.GetItem(1);
        VERIFY(itemRecord.Valid());
        VERIFY(!strcmp(itemRecord.Get().name.ptr, "Long Sword"));
        VERIFY(itemRecord.Get().sellCost == 5);
        VERIFY(itemRecord.Get().buyCost == 50);
    }

    // test index 2
    {
        ShopData::ItemRecord itemRecord = data.GetItem(2);
        VERIFY(itemRecord.Valid());
        VERIFY(!strcmp(itemRecord.Get().name.ptr, "Rusty Dagger"));
        VERIFY(itemRecord.Get().sellCost == 1);
        VERIFY(itemRecord.Get().buyCost == 10);
    }

    // test an invalid index
    {
        VERIFY(!data.GetItem(3).Valid());
    }

    // test getting by name
    {
        ShopData::ItemRecord itemRecord = data.GetItem("Rusty Dagger");
        VERIFY(itemRecord.Valid());
        VERIFY(!strcmp(itemRecord.Get().name.ptr, "Rusty Dagger"));
    }

    printf("All checks passed!\n");
    return 0;
}
