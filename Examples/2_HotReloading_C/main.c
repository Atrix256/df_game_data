#define DF_GAMEDATA_IMPLEMENTATION
#include "Data_C.h"

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
    Data_C_Database data;
    Data_C_InitDatabase(&data);

    if (!Data_C_LoadFromFile("packed/Data.bin", &data))
    {
        printf("Could not load packed/Data.bin\n");
        return 1;
    }

    // Characters
    VERIFY(data.m_table_Character_count == 3);
    Data_C_Character* characters = (Data_C_Character*)data.m_table_Character;

    Data_C_Character* char0 = &characters[0];
    VERIFY(!strcmp((const char*)char0->name, "Count Tyrone Rugen"));
    VERIFY(char0->inventory_count == 1);
    VERIFY(((Data_C_Item**)char0->inventory)[0] != NULL);
    VERIFY(!strcmp((const char*)((Data_C_Item**)char0->inventory)[0]->name, "6 Fingers"));

    Data_C_Character* char1 = &characters[1];
    VERIFY(!strcmp((const char*)char1->name, "Dread Pirate Roberts"));
    VERIFY(char1->inventory_count == 2);
    VERIFY(((Data_C_Item**)char1->inventory)[0] != NULL);
    VERIFY(((Data_C_Item**)char1->inventory)[1] != NULL);
    VERIFY(!strcmp((const char*)((Data_C_Item**)char1->inventory)[0]->name, "Black Mask"));
    VERIFY(!strcmp((const char*)((Data_C_Item**)char1->inventory)[1]->name, "True Love"));

    Data_C_Character* char2 = &characters[2];
    VERIFY(!strcmp((const char*)char2->name, "Inigo Montoya"));
    VERIFY(char2->inventory_count == 1);
    VERIFY(((Data_C_Item**)char2->inventory)[0] != NULL);
    VERIFY(!strcmp((const char*)((Data_C_Item**)char2->inventory)[0]->name, "Father's Sword"));

    // Items
    VERIFY(data.m_table_Item_count == 4);
    Data_C_Item* items = (Data_C_Item*)data.m_table_Item;

    Data_C_Item* item0 = &items[0];
    VERIFY(!strcmp((const char*)item0->name, "6 Fingers"));
    VERIFY(item0->value == 0.0f);

    Data_C_Item* item1 = &items[1];
    VERIFY(!strcmp((const char*)item1->name, "Black Mask"));
    VERIFY(item1->value == 10.0f);

    Data_C_Item* item2 = &items[2];
    VERIFY(!strcmp((const char*)item2->name, "Father's Sword"));
    VERIFY(item2->value == 1000.0f);

    Data_C_Item* item3 = &items[3];
    VERIFY(!strcmp((const char*)item3->name, "True Love"));
    VERIFY(item3->value == 10.0f);

    if (argc >= 2 && !strcmp(argv[1], "-test"))
    {
        printf("All checks passed!\n");
        return 0;
    }

    printf("Hot reloading is watching for updates. Edit and recompile source data. Press Q to exit.\n\n");


    // TODO: finish porting the tests

    printf("All checks passed!\n");

    Data_C_DestroyDatabase(&data);

    return 0;
}
// TODO: make hot reloading work if -test not given, like the cpp version
// TODO: DoEndianSwapAndPointerFixup. Arrays need to do their count, and then do their items in a loop
// TODO: inventory is an array of links.  here it is an array of uint64 in the comments. should fix those comments
// TODO: take m_ off things for less typing
// TODO: maybe you should make a union type for each pointer type needed? Better type safety and easier to understand
// TODO: look for TODOs
