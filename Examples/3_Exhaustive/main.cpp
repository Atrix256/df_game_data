
#include "Data.h"

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
        printf("Could not load packed/Data.bin");
        return 1;
    }

    VERIFY(data.GetEntryCount() == 1);

    const Data::Entry& entry = data.GetEntry(0).Get();

    VERIFY(entry.entries._bool.Get() == true);
    VERIFY(entry.entries._uint8 == 2);
    VERIFY(entry.entries._sint8 == -3);
    VERIFY(entry.entries._uint16 == 4);
    VERIFY(entry.entries._sint16 == -5);
    VERIFY(entry.entries._uint32 == 6);
    VERIFY(entry.entries._sint32 == -7);
    VERIFY(entry.entries._uint64 == 8);
    VERIFY(entry.entries._sint64 == -9);
    VERIFY(entry.entries._float == 10.0f);
    VERIFY(entry.entries._double == -11.0);
    VERIFY(entry.entries._color == Data::Color::Blue);
    VERIFY(entry.entries._pos.x == 2.0f);
    VERIFY(entry.entries._pos.y == 1.0f);

    VERIFY(entry.entries_defaults._bool.Get() == true);
    VERIFY(entry.entries_defaults._uint8 == 1);
    VERIFY(entry.entries_defaults._sint8 == -2);
    VERIFY(entry.entries_defaults._uint16 == 3);
    VERIFY(entry.entries_defaults._sint16 == -4);
    VERIFY(entry.entries_defaults._uint32 == 5);
    VERIFY(entry.entries_defaults._sint32 == -6);
    VERIFY(entry.entries_defaults._uint64 == 7);
    VERIFY(entry.entries_defaults._sint64 == -8);
    VERIFY(entry.entries_defaults._float == 10000.0f);
    VERIFY(entry.entries_defaults._double == 1000.0);
    VERIFY(entry.entries_defaults._color == Data::Color::Hazel);

    printf("All checks passed!");
    return 0;
}
