#include <stdio.h>
#include <vector>
#include <conio.h>
#include <thread>
#include <chrono>

#include "data/data_debug.h"
#include "data/data_release.h"

#ifdef _DEBUG
using Data = DataDebug;
static const char* s_dataFile = "data/data_debug.bin";
#else
using Data = DataRelease;
static const char* s_dataFile = "data/data_release.bin";
#endif

int main(int argc, char** argv)
{
    Data data;
    if (!data.LoadFromFile(s_dataFile))
    {
        printf("Could not load data!\n");
        return 1;
    }

    std::vector<Data::CharacterRecord> characters;
    for (uint32_t i = 0; i < data.GetCount<Data::Character>() + 1; ++i)
        characters.push_back(data.Get<Data::Character>(i));

    std::vector<Data::ItemRecord> items;
    for (uint32_t i = 0; i < data.GetCount<Data::Item>(); ++i)
        items.push_back(data.Get<Data::Item>(i));

    const Data::Character& a = characters[0].Get();
    const Data::Character& b = characters[1].Get();
    const Data::Character& c = characters[2].Get();
    const Data::Character& d = characters[3].Get();

#ifdef _DEBUG
    auto test1 = data.Get<Data::Character>("Larry");
    auto test2 = data.Get<Data::Character>("larry");
#endif

    printf("Program watching input file for updates. Press Q to exit...\n");

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
            printf("File changed!\n");
            #ifdef _DEBUG
            printf("entry[\"larry\"].name = %s\n", test2.Get().name.ptr);
            #endif
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // TODO: print out some data?
    // TODO: how are we going to test the "test" data set? maybe a separate project?
    return 0;
}
