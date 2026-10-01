
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

    // A bunch of types
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
    VERIFY(!strcmp(entry.entries._string.ptr, "some text!"));
    VERIFY(entry.entries._color == Data::Color::Blue);
    VERIFY(entry.entries._pos.x == 2.0f);
    VERIFY(entry.entries._pos.y == 1.0f);
    VERIFY(entry.entries._people.ptr == nullptr);
    VERIFY(entry.entries._UTest.type == Data::UTest_type::_float);
    VERIFY(*entry.entries._UTest._float() == 23.0f);

    // A bunch of types which have default values set in the def file
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
    VERIFY(!strcmp(entry.entries_defaults._string.ptr, "yo!"));
    VERIFY(entry.entries_defaults._color == Data::Color::Hazel);
    VERIFY(entry.entries_defaults._people.ptr != nullptr);
    VERIFY(!strcmp(entry.entries_defaults._people.ptr->name.ptr, "Larry"));

    // A bunch of dynamic arrays of types
    VERIFY(entry.entries_dynamic_arrays._bool_count == 3);
    VERIFY(entry.entries_dynamic_arrays._bool.ptr[0].Get() == false);
    VERIFY(entry.entries_dynamic_arrays._bool.ptr[1].Get() == true);
    VERIFY(entry.entries_dynamic_arrays._bool.ptr[2].Get() == false);
    VERIFY(entry.entries_dynamic_arrays._uint8_count == 1);
    VERIFY(entry.entries_dynamic_arrays._uint8.ptr[0] == 2);
    VERIFY(entry.entries_dynamic_arrays._sint8_count == 1);
    VERIFY(entry.entries_dynamic_arrays._sint8.ptr[0] == 4);
    VERIFY(entry.entries_dynamic_arrays._uint16_count == 1);
    VERIFY(entry.entries_dynamic_arrays._uint16.ptr[0] == 6);
    VERIFY(entry.entries_dynamic_arrays._sint16_count == 1);
    VERIFY(entry.entries_dynamic_arrays._sint16.ptr[0] == 8);
    VERIFY(entry.entries_dynamic_arrays._uint32_count == 1);
    VERIFY(entry.entries_dynamic_arrays._uint32.ptr[0] == 10);
    VERIFY(entry.entries_dynamic_arrays._sint32_count == 1);
    VERIFY(entry.entries_dynamic_arrays._sint32.ptr[0] == 12);
    VERIFY(entry.entries_dynamic_arrays._uint64_count == 1);
    VERIFY(entry.entries_dynamic_arrays._uint64.ptr[0] == 14);
    VERIFY(entry.entries_dynamic_arrays._sint64_count == 2);
    VERIFY(entry.entries_dynamic_arrays._sint64.ptr[0] == 16);
    VERIFY(entry.entries_dynamic_arrays._sint64.ptr[1] == 18);
    VERIFY(entry.entries_dynamic_arrays._float_count == 1);
    VERIFY(entry.entries_dynamic_arrays._float.ptr[0] == 20.0f);
    VERIFY(entry.entries_dynamic_arrays._double_count == 1);
    VERIFY(entry.entries_dynamic_arrays._double.ptr[0] == 22.0f);
    VERIFY(entry.entries_dynamic_arrays._string_count == 3);
    VERIFY(!strcmp(entry.entries_dynamic_arrays._string.ptr[0].ptr, "uno"));
    VERIFY(!strcmp(entry.entries_dynamic_arrays._string.ptr[1].ptr, ""));
    VERIFY(!strcmp(entry.entries_dynamic_arrays._string.ptr[2].ptr, "tres"));
    VERIFY(entry.entries_dynamic_arrays._color_count == 2);
    VERIFY(entry.entries_dynamic_arrays._color.ptr[0] == Data::Color::Blue);
    VERIFY(entry.entries_dynamic_arrays._color.ptr[1] == Data::Color::Green);
    VERIFY(entry.entries_dynamic_arrays._pos_count == 2);
    VERIFY(entry.entries_dynamic_arrays._pos.ptr[0].x == 96.0f);
    VERIFY(entry.entries_dynamic_arrays._pos.ptr[0].y == 48.0f);
    VERIFY(entry.entries_dynamic_arrays._pos.ptr[1].x == 7.0f);
    VERIFY(entry.entries_dynamic_arrays._pos.ptr[1].y == 12.0f);
    VERIFY(entry.entries_dynamic_arrays._people_count == 3);
    VERIFY(entry.entries_dynamic_arrays._people.ptr[0].ptr != nullptr);
    VERIFY(!strcmp(entry.entries_dynamic_arrays._people.ptr[0].ptr->name.ptr, "Larry"));
    VERIFY(entry.entries_dynamic_arrays._people.ptr[1].ptr == nullptr);
    VERIFY(entry.entries_dynamic_arrays._people.ptr[2].ptr != nullptr);
    VERIFY(!strcmp(entry.entries_dynamic_arrays._people.ptr[2].ptr->name.ptr, "Moe"));
    VERIFY(entry.entries_dynamic_arrays._UTest_count == 3);
    VERIFY(entry.entries_dynamic_arrays._UTest.ptr[0].type == Data::UTest_type::None);
    VERIFY(entry.entries_dynamic_arrays._UTest.ptr[1].type == Data::UTest_type::_pos);
    VERIFY(entry.entries_dynamic_arrays._UTest.ptr[1]._pos()->x == 2.0f);
    VERIFY(entry.entries_dynamic_arrays._UTest.ptr[1]._pos()->y == 3.0f);
    VERIFY(entry.entries_dynamic_arrays._UTest.ptr[2].type == Data::UTest_type::_float);
    VERIFY(*entry.entries_dynamic_arrays._UTest.ptr[2]._float() == 4.0f);

    // A bunch of static arrays of types
    VERIFY(entry.entries_static_arrays._bool_count == 2);
    VERIFY(entry.entries_static_arrays._bool[0].Get() == true);
    VERIFY(entry.entries_static_arrays._bool[1].Get() == false);
    VERIFY(entry.entries_static_arrays._uint8_count == 2);
    VERIFY(entry.entries_static_arrays._uint8[0] == 4);
    VERIFY(entry.entries_static_arrays._uint8[1] == 3);
    VERIFY(entry.entries_static_arrays._sint8_count == 2);
    VERIFY(entry.entries_static_arrays._sint8[0] == 8);
    VERIFY(entry.entries_static_arrays._sint8[1] == 6);
    VERIFY(entry.entries_static_arrays._uint16_count == 2);
    VERIFY(entry.entries_static_arrays._uint16[0] == 1);
    VERIFY(entry.entries_static_arrays._uint16[1] == 2);
    VERIFY(entry.entries_static_arrays._sint16_count == 2);
    VERIFY(entry.entries_static_arrays._sint16[0] == 2);
    VERIFY(entry.entries_static_arrays._sint16[1] == 4);
    VERIFY(entry.entries_static_arrays._uint32_count == 2);
    VERIFY(entry.entries_static_arrays._uint32[0] == 3);
    VERIFY(entry.entries_static_arrays._uint32[1] == 1);
    VERIFY(entry.entries_static_arrays._sint32_count == 2);
    VERIFY(entry.entries_static_arrays._sint32[0] == 5);
    VERIFY(entry.entries_static_arrays._sint32[1] == 1);
    VERIFY(entry.entries_static_arrays._uint32_count == 2);
    VERIFY(entry.entries_static_arrays._uint64[0] == 2);
    VERIFY(entry.entries_static_arrays._uint64[1] == 4);
    VERIFY(entry.entries_static_arrays._sint64_count == 2);
    VERIFY(entry.entries_static_arrays._sint64[0] == 9);
    VERIFY(entry.entries_static_arrays._sint64[1] == 8);
    VERIFY(entry.entries_static_arrays._float_count == 2);
    VERIFY(entry.entries_static_arrays._float[0] == 6.0f);
    VERIFY(entry.entries_static_arrays._float[1] == 7.0f);
    VERIFY(entry.entries_static_arrays._double_count == 2);
    VERIFY(entry.entries_static_arrays._double[0] == 11.0f);
    VERIFY(entry.entries_static_arrays._double[1] == 41.0f);
    VERIFY(entry.entries_static_arrays._string_count == 2);
    VERIFY(!strcmp(entry.entries_static_arrays._string[0].ptr, "un"));
    VERIFY(!strcmp(entry.entries_static_arrays._string[1].ptr, "deux"));
    VERIFY(entry.entries_static_arrays._color_count == 2);
    VERIFY(entry.entries_static_arrays._color[0] == Data::Color::Hazel);
    VERIFY(entry.entries_static_arrays._color[1] == Data::Color::Blue);
    VERIFY(entry.entries_static_arrays._pos_count == 2);
    VERIFY(entry.entries_static_arrays._pos[0].x == 4.0f);
    VERIFY(entry.entries_static_arrays._pos[0].y == 3.0f);
    VERIFY(entry.entries_static_arrays._pos[1].x == 5.0f);
    VERIFY(entry.entries_static_arrays._pos[1].y == 9.0f);
    VERIFY(entry.entries_static_arrays._people_count == 2);
    VERIFY(entry.entries_static_arrays._people[0].ptr == nullptr);
    VERIFY(entry.entries_static_arrays._people[1].ptr != nullptr);
    VERIFY(!strcmp(entry.entries_static_arrays._people[1].ptr->name.ptr, "Curly"));
    VERIFY(entry.entries_static_arrays._UTest_count == 2);
    VERIFY(entry.entries_static_arrays._UTest[0].type == Data::UTest_type::_float);
    VERIFY(*entry.entries_static_arrays._UTest[0]._float() == 99.0f);
    VERIFY(entry.entries_static_arrays._UTest[1].type == Data::UTest_type::None);

    printf("All checks passed!");
    return 0;
}
