#include <stdio.h>
#include <vector>

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
    for (uint32_t i = 0; i < data.GetCharacterCount() + 1; ++i)
        characters.push_back(data.GetCharacter(i));

    std::vector<Data::ItemRecord> items;
    for (uint32_t i = 0; i < data.GetItemCount(); ++i)
        items.push_back(data.GetItem(i));

    const Data::Character& a = characters[0].Get();
    const Data::Character& b = characters[1].Get();
    const Data::Character& c = characters[2].Get();
    const Data::Character& d = characters[3].Get();

#ifdef _DEBUG
    auto test1 = data.GetCharacter("Larry");
    auto test2 = data.GetCharacter("larry");
#endif

    // TODO: print out some data?
    // TODO: how are we going to test the "test" data set? maybe a separate project?
    return 0;
}
