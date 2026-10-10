#define DF_GAMEDATA_IMPLEMENTATION
#include "Data_C.h"

#include <conio.h>
#include <time.h>
#include <threads.h>

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
    VERIFY(data.table_Character_count == 3);
    const Data_C_Character* characters = (Data_C_Character*)data.table_Character;

    const Data_C_Character* char0 = &characters[0];
    VERIFY(!strcmp((const char*)char0->name, "Count Tyrone Rugen"));
    VERIFY(char0->inventory_count == 1);
    VERIFY(((Data_C_Item**)char0->inventory)[0] != NULL);
    VERIFY(!strcmp((const char*)((Data_C_Item**)char0->inventory)[0]->name, "6 Fingers"));

    const Data_C_Character* char1 = &characters[1];
    VERIFY(!strcmp((const char*)char1->name, "Dread Pirate Roberts"));
    VERIFY(char1->inventory_count == 2);
    VERIFY(((Data_C_Item**)char1->inventory)[0] != NULL);
    VERIFY(((Data_C_Item**)char1->inventory)[1] != NULL);
    VERIFY(!strcmp((const char*)((Data_C_Item**)char1->inventory)[0]->name, "Black Mask"));
    VERIFY(!strcmp((const char*)((Data_C_Item**)char1->inventory)[1]->name, "True Love"));

    const Data_C_Character* char2 = &characters[2];
    VERIFY(!strcmp((const char*)char2->name, "Inigo Montoya"));
    VERIFY(char2->inventory_count == 1);
    VERIFY(((Data_C_Item**)char2->inventory)[0] != NULL);
    VERIFY(!strcmp((const char*)((Data_C_Item**)char2->inventory)[0]->name, "Father's Sword"));

    // Items
    VERIFY(data.table_Item_count == 4);
    const Data_C_Item* items = (Data_C_Item*)data.table_Item;

    const Data_C_Item* item0 = &items[0];
    VERIFY(!strcmp((const char*)item0->name, "6 Fingers"));
    VERIFY(item0->value == 0.0f);

    const Data_C_Item* item1 = &items[1];
    VERIFY(!strcmp((const char*)item1->name, "Black Mask"));
    VERIFY(item1->value == 10.0f);

    const Data_C_Item* item2 = &items[2];
    VERIFY(!strcmp((const char*)item2->name, "Father's Sword"));
    VERIFY(item2->value == 1000.0f);

    const Data_C_Item* item3 = &items[3];
    VERIFY(!strcmp((const char*)item3->name, "True Love"));
    VERIFY(item3->value == 10.0f);

    if (argc >= 2 && !strcmp(argv[1], "-test"))
    {
        printf("All checks passed!\n");
        return 0;
    }

    printf("Hot reloading is watching for updates. Edit and recompile source data. Press Q to exit.\n\n");

    bool showData = true;
    while (true)
    {
        if (_kbhit())
        {
            int ch = _getch();
            if (ch == 'q')
                break;
        }

        if (Data_C_Tick(&data))
        {
            showData = true;
            printf("\nData Changed! Press Q to exit.\n\n");
        }

        if (showData)
        {
            showData = false;

            // Show character data
            uint32_t characterCount = data.table_Character_count;
            printf("%u Characters:\n", characterCount);
            for (uint32_t i = 0; i < characterCount; ++i)
            {
                const Data_C_Character* character = &characters[i];
                uint32_t inventoryCount = character->inventory_count;
                printf("    [%u] %s has %u items\n", i, (const char*)character->name, inventoryCount);
                for (uint32_t j = 0; j < inventoryCount; ++j)
                {
                    const char* itemName = NULL;
                    if (((Data_C_Item**)character->inventory)[j])
                        itemName = (const char*)((Data_C_Item**)character->inventory)[j]->name;

                    printf("        [%u] %s\n", j, itemName);
                }
            }

            // Show item data
            uint32_t itemCount = data.table_Item_count;
            printf("%u Items:\n", itemCount);
            for (uint32_t i = 0; i < itemCount; ++i)
            {
                const Data_C_Item* item = &items[i];
                printf("    [%u] %s %f\n", i, (const char*)item->name, item->value);
            }
        }

        // Make this thread sleep for 250 milliseconds.
        // Hot reloading checks for changes 4 times a second.
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = 250 * 1000 * 1000; // 250 milliseconds in nanoseconds
        thrd_sleep(&ts, NULL);
    }

    printf("\nAs you wish!\n");

    Data_C_DestroyDatabase(&data);

    return 0;
}

// TODO: inventory is an array of links.  here it is an array of uint64 in the comments. should fix those comments
// TODO: maybe you should make a union type for each pointer type needed? Better type safety and easier to understand
// TODO: look for TODOs
