#include <stdio.h>

#include "data/data.h"

int main(int argc, char** argv)
{
    dfgd data;
    if (!data.LoadFromFile("data/data.bin"))
    {
        printf("Could not load data!\n");
        return 1;
    }

    auto blah1 = data.m_table_Character_names.Get();
    auto blah2 = data.m_table_Character.Get();

    auto blah3 = data.m_table_Item_names.Get();
    auto blah4 = data.m_table_Item.Get();

    // TODO: load data
    return 0;
}
