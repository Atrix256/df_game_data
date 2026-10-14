#define DF_GAMEDATA_IMPLEMENTATION
#include "Data_C.h"

#include <conio.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#define VERIFY(x) if (!Verify(x, #x, __FILE__, __LINE__)) return 1;

bool Verify(bool value, const char* cond, const char* fileName, int line)
{
    if (value)
        return true;

    printf("Condition failed (%s %i): %s\n", fileName, line, cond);

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
    const Data_C_Character* characters = data.table_Character;

    const Data_C_Character* char0 = &characters[0];
    VERIFY(!strcmp(char0->name.ptr, "Count Tyrone Rugen"));
    VERIFY(char0->inventory_count == 1);
    VERIFY(char0->inventory.ptr[0].ptr != NULL);
    VERIFY(!strcmp(char0->inventory.ptr[0].ptr->name.ptr, "6 Fingers"));

    const Data_C_Character* char1 = &characters[1];
    VERIFY(!strcmp(char1->name.ptr, "Dread Pirate Roberts"));
    VERIFY(char1->inventory_count == 2);
    VERIFY(char1->inventory.ptr[0].ptr != NULL);
    VERIFY(char1->inventory.ptr[1].ptr != NULL);
    VERIFY(!strcmp(char1->inventory.ptr[0].ptr->name.ptr, "Black Mask"));
    VERIFY(!strcmp(char1->inventory.ptr[1].ptr->name.ptr, "True Love"));

    const Data_C_Character* char2 = &characters[2];
    VERIFY(!strcmp(char2->name.ptr, "Inigo Montoya"));
    VERIFY(char2->inventory_count == 1);
    VERIFY(char2->inventory.ptr[0].ptr != NULL);
    VERIFY(!strcmp(char2->inventory.ptr[0].ptr->name.ptr, "Father's Sword"));

    // Items
    VERIFY(data.table_Item_count == 4);
    const Data_C_Item* items = data.table_Item;

    const Data_C_Item* item0 = &items[0];
    VERIFY(!strcmp(item0->name.ptr, "6 Fingers"));
    VERIFY(item0->value == 0.0f);

    const Data_C_Item* item1 = &items[1];
    VERIFY(!strcmp(item1->name.ptr, "Black Mask"));
    VERIFY(item1->value == 10.0f);

    const Data_C_Item* item2 = &items[2];
    VERIFY(!strcmp(item2->name.ptr, "Father's Sword"));
    VERIFY(item2->value == 1000.0f);

    const Data_C_Item* item3 = &items[3];
    VERIFY(!strcmp(item3->name.ptr, "True Love"));
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
                printf("    [%u] %s has %u items\n", i, character->name.ptr, inventoryCount);
                for (uint32_t j = 0; j < inventoryCount; ++j)
                {
                    const char* itemName = NULL;
                    if (character->inventory.ptr[j].ptr)
                        itemName = character->inventory.ptr[j].ptr->name.ptr;

                    printf("        [%u] %s\n", j, itemName);
                }
            }

            // Show item data
            uint32_t itemCount = data.table_Item_count;
            printf("%u Items:\n", itemCount);
            for (uint32_t i = 0; i < itemCount; ++i)
            {
                const Data_C_Item* item = &items[i];
                printf("    [%u] %s %f\n", i, item->name.ptr, item->value);
            }
        }

        // Make this thread sleep for 250 milliseconds.
        // Hot reloading checks for changes 4 times a second.
        Sleep(250);
    }

    printf("\nAs you wish!\n");

    Data_C_DestroyDatabase(&data);

    return 0;
}
