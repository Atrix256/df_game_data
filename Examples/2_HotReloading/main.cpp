
#include "Data.h"
#include <conio.h>
#include <thread>
#include <chrono>

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
        printf("Could not load packed/Data.bin\n");
        return 1;
    }

    VERIFY(data.GetCharacterCount() == 3);

    Data::CharacterRecord char0 = data.GetCharacter(0);
    VERIFY(!strcmp(char0.Get().name.ptr, "Count Tyrone Rugen"));
    VERIFY(char0.Get().inventory_count == 1);
    VERIFY(char0.Get().inventory.ptr[0].ptr != nullptr);
    VERIFY(!strcmp(char0.Get().inventory.ptr[0].ptr->name.ptr, "6 Fingers"));

    Data::CharacterRecord char1 = data.GetCharacter(1);
    VERIFY(!strcmp(char1.Get().name.ptr, "Dread Pirate Roberts"));
    VERIFY(char1.Get().inventory_count == 2);
    VERIFY(char1.Get().inventory.ptr[0].ptr != nullptr);
    VERIFY(char1.Get().inventory.ptr[1].ptr != nullptr);
    VERIFY(!strcmp(char1.Get().inventory.ptr[0].ptr->name.ptr, "Black Mask"));
    VERIFY(!strcmp(char1.Get().inventory.ptr[1].ptr->name.ptr, "True Love"));

    Data::CharacterRecord char2 = data.GetCharacter(2);
    VERIFY(!strcmp(char2.Get().name.ptr, "Inigo Montoya"));
    VERIFY(char2.Get().inventory_count == 1);
    VERIFY(char2.Get().inventory.ptr[0].ptr != nullptr);
    VERIFY(!strcmp(char2.Get().inventory.ptr[0].ptr->name.ptr, "Father's Sword"));

    VERIFY(data.GetItemCount() == 4);
    Data::ItemRecord item0 = data.GetItem(0);
    VERIFY(!strcmp(item0.Get().name.ptr, "6 Fingers"));
    VERIFY(item0.Get().value == 0.0f);

    Data::ItemRecord item1 = data.GetItem(1);
    VERIFY(!strcmp(item1.Get().name.ptr, "Black Mask"));
    VERIFY(item1.Get().value == 10.0f);

    Data::ItemRecord item2 = data.GetItem(2);
    VERIFY(!strcmp(item2.Get().name.ptr, "Father's Sword"));
    VERIFY(item2.Get().value == 1000.0f);

    Data::ItemRecord item3 = data.GetItem(3);
    VERIFY(!strcmp(item3.Get().name.ptr, "True Love"));
    VERIFY(item3.Get().value == 10.0f);

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

        if (data.Tick())
        {
            showData = true;
            printf("\nData Changed! Press Q to exit.\n\n");
        }

        if (showData)
        {
            showData = false;

            // Show character data
            uint32_t characterCount = data.GetCharacterCount();
            printf("%u Characters:\n", characterCount);
            for (uint32_t i = 0; i < characterCount; ++i)
            {
                Data::CharacterRecord record = data.GetCharacter(i);
                uint32_t inventoryCount = record.Get().inventory_count;
                printf("    [%u] %s has %u items\n", i, record.Get().name.ptr, inventoryCount);
                for (uint32_t j = 0; j < inventoryCount; ++j)
                {
                    const char* itemName = nullptr;
                    if (record.Get().inventory.ptr[j].ptr)
                        itemName = record.Get().inventory.ptr[j].ptr->name.ptr;

                    printf("        [%u] %s\n", j, itemName);
                }
            }

            // Show item data
            uint32_t itemCount = data.GetItemCount();
            printf("%u Items:\n", itemCount);
            for (uint32_t i = 0; i < itemCount; ++i)
            {
                Data::ItemRecord record = data.GetItem(i);
                const Data::Item& item = record.Get();
                printf("    [%u] %s %f\n", i, item.name.ptr, item.value);
            }
        }

        // Make this thread sleep for 250 milliseconds.
        // Hot reloading checks for changes 4 times a second.
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    printf("\nAs you wish!\n");
    return 0;
}
