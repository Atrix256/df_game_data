#include <stdio.h>
#include <vector>

#include "data/data.h"

int main(int argc, char** argv)
{
    dfgd data;
    if (!data.LoadFromFile("data/data.bin"))
    {
        printf("Could not load data!\n");
        return 1;
    }

    std::vector<dfgd::CharacterRecord> characters;
    for (uint32_t i = 0; i < data.GetCharacterCount() + 1; ++i)
        characters.push_back(data.GetCharacter(i));

    std::vector<dfgd::ItemRecord> items;
    for (uint32_t i = 0; i < data.GetItemCount(); ++i)
        items.push_back(data.GetItem(i));

    const dfgd::Character& a = characters[0].Get();
    const dfgd::Character& b = characters[1].Get();
    const dfgd::Character& c = characters[2].Get();
    const dfgd::Character& d = characters[3].Get();

    // TODO: load data
    return 0;
}
