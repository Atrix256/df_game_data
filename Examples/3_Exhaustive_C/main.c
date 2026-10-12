#define DF_GAMEDATA_IMPLEMENTATION
#include "Data_C.h"
#include "Data_No_Obfuscation_C.h"

#define VERIFY(x) if (!Verify(x, #x, __FILE__, __LINE__)) return 1;

bool Verify(bool value, const char* cond, const char* fileName, int line)
{
    if (value)
        return true;

    printf("Condition failed (%s %i): %s\n", fileName, line, cond);

    return false;
}

bool DoTest_Data(const char* fileName)
{
    Data_C_Database data;
    Data_C_InitDatabase(&data);

    if (!Data_C_LoadFromFile(fileName, &data))
    {
        printf("Could not load %s\n", fileName);
        return 1;
    }

    printf("%s\n", fileName);

    VERIFY(data.table_Entry_count == 1);
    const Data_C_Entry* entries = (const Data_C_Entry*)data.table_Entry;

    const Data_C_Entry* entry = &entries[0];

    // A bunch of types
    VERIFY(entry->entries._bool == true);
    VERIFY(entry->entries._uint8 == 2);
    VERIFY(entry->entries._sint8 == -3);
    VERIFY(entry->entries._uint16 == 4);
    VERIFY(entry->entries._sint16 == -5);
    VERIFY(entry->entries._uint32 == 6);
    VERIFY(entry->entries._sint32 == -7);
    VERIFY(entry->entries._uint64 == 8);
    VERIFY(entry->entries._sint64 == -9);
    VERIFY(entry->entries._float == 10.0f);
    VERIFY(entry->entries._double == -11.0);
    VERIFY(!strcmp((const char*)entry->entries._string, "some text!"));
    VERIFY(entry->entries._color == Data_C_Color_Blue);
    VERIFY(entry->entries._pos.x == 2.0f);
    VERIFY(entry->entries._pos.y == 1.0f);
    VERIFY((const Data_C_People*)(entry->entries._people) == NULL);
    VERIFY(entry->entries._UTest.type == Data_C_UTest_type__float);
    VERIFY(*(float*)entry->entries._UTest.ptr == 23.0f);

    // A bunch of types which have default values set in the def file
    VERIFY(entry->entries_defaults._bool == true);
    VERIFY(entry->entries_defaults._uint8 == 1);
    VERIFY(entry->entries_defaults._sint8 == -2);
    VERIFY(entry->entries_defaults._uint16 == 3);
    VERIFY(entry->entries_defaults._sint16 == -4);
    VERIFY(entry->entries_defaults._uint32 == 5);
    VERIFY(entry->entries_defaults._sint32 == -6);
    VERIFY(entry->entries_defaults._uint64 == 7);
    VERIFY(entry->entries_defaults._sint64 == -8);
    VERIFY(entry->entries_defaults._float == 10000.0f);
    VERIFY(entry->entries_defaults._double == 1000.0);
    VERIFY(!strcmp((const char*)entry->entries_defaults._string, "yo!"));
    VERIFY(entry->entries_defaults._color == Data_C_Color_Hazel);
    VERIFY((const Data_C_People*)(entry->entries_defaults._people) != NULL);
    VERIFY(!strcmp((const char*)((const Data_C_People*)(entry->entries_defaults._people))->name, "Larry"));

    // A bunch of dynamic arrays of types
    VERIFY(entry->entries_dynamic_arrays._bool_count == 3);
    VERIFY(((uint8_t*)entry->entries_dynamic_arrays._bool)[0] == false);
    VERIFY(((uint8_t*)entry->entries_dynamic_arrays._bool)[1] == true);
    VERIFY(((uint8_t*)entry->entries_dynamic_arrays._bool)[2] == false);
    VERIFY(entry->entries_dynamic_arrays._uint8_count == 1);
    VERIFY(((uint8_t*)entry->entries_dynamic_arrays._uint8)[0] == 2);
    VERIFY(entry->entries_dynamic_arrays._sint8_count == 1);
    VERIFY(((int8_t*)entry->entries_dynamic_arrays._sint8)[0] == 4);
    VERIFY(entry->entries_dynamic_arrays._uint16_count == 1);
    VERIFY(((uint16_t*)entry->entries_dynamic_arrays._uint16)[0] == 6);
    VERIFY(entry->entries_dynamic_arrays._sint16_count == 1);
    VERIFY(((int16_t*)entry->entries_dynamic_arrays._sint16)[0] == 8);
    VERIFY(entry->entries_dynamic_arrays._uint32_count == 1);
    VERIFY(((uint32_t*)entry->entries_dynamic_arrays._uint32)[0] == 10);
    VERIFY(entry->entries_dynamic_arrays._sint32_count == 1);
    VERIFY(((int32_t*)entry->entries_dynamic_arrays._sint32)[0] == 12);
    VERIFY(entry->entries_dynamic_arrays._uint64_count == 1);
    VERIFY(((uint64_t*)entry->entries_dynamic_arrays._uint64)[0] == 14);
    VERIFY(entry->entries_dynamic_arrays._sint64_count == 2);
    VERIFY(((int64_t*)entry->entries_dynamic_arrays._sint64)[0] == 16);
    VERIFY(((int64_t*)entry->entries_dynamic_arrays._sint64)[1] == 18);
    VERIFY(entry->entries_dynamic_arrays._float_count == 1);
    VERIFY(((float*)entry->entries_dynamic_arrays._float)[0] == 20.0f);
    VERIFY(entry->entries_dynamic_arrays._double_count == 1);
    VERIFY(((double*)entry->entries_dynamic_arrays._double)[0] == 22.0f);
    VERIFY(entry->entries_dynamic_arrays._string_count == 3);
    VERIFY(!strcmp((char*)(((uint64_t*)entry->entries_dynamic_arrays._string)[0]), "uno"));
    VERIFY(!strcmp((char*)(((uint64_t*)entry->entries_dynamic_arrays._string)[1]), ""));
    VERIFY(!strcmp((char*)(((uint64_t*)entry->entries_dynamic_arrays._string)[2]), "tres"));
    VERIFY(entry->entries_dynamic_arrays._color_count == 2);
    VERIFY(((uint16_t*)entry->entries_dynamic_arrays._color)[0] == Data_C_Color_Blue);
    VERIFY(((uint16_t*)entry->entries_dynamic_arrays._color)[1] == Data_C_Color_Green);
    VERIFY(entry->entries_dynamic_arrays._pos_count == 2);
    VERIFY(((Data_C_Pos*)entry->entries_dynamic_arrays._pos)[0].x == 96.0f);
    VERIFY(((Data_C_Pos*)entry->entries_dynamic_arrays._pos)[0].y == 48.0f);
    VERIFY(((Data_C_Pos*)entry->entries_dynamic_arrays._pos)[1].x == 7.0f);
    VERIFY(((Data_C_Pos*)entry->entries_dynamic_arrays._pos)[1].y == 12.0f);
    VERIFY(entry->entries_dynamic_arrays._people_count == 3);
    VERIFY((const Data_C_People*)(((uint64_t*)entry->entries_dynamic_arrays._people)[0]) != NULL);
    VERIFY(!strcmp((const char*)((const Data_C_People*)(((uint64_t*)entry->entries_dynamic_arrays._people)[0]))->name, "Larry"));
    VERIFY((const Data_C_People*)(((uint64_t*)entry->entries_dynamic_arrays._people)[1]) == NULL);
    VERIFY((const Data_C_People*)(((uint64_t*)entry->entries_dynamic_arrays._people)[2]) != NULL);
    VERIFY(!strcmp((const char*)((const Data_C_People*)(((uint64_t*)entry->entries_dynamic_arrays._people)[2]))->name, "Moe"));
    VERIFY(entry->entries_dynamic_arrays._UTest_count == 3);
    VERIFY((((Data_C_UTest*)entry->entries_dynamic_arrays._UTest)[0]).type == Data_C_UTest_type_None);
    VERIFY((((Data_C_UTest*)entry->entries_dynamic_arrays._UTest)[1]).type == Data_C_UTest_type__pos);
    VERIFY(((Data_C_Pos*)((((Data_C_UTest*)entry->entries_dynamic_arrays._UTest)[1]).ptr))->x == 2.0f);
    VERIFY(((Data_C_Pos*)((((Data_C_UTest*)entry->entries_dynamic_arrays._UTest)[1]).ptr))->y == 3.0f);
    VERIFY((((Data_C_UTest*)entry->entries_dynamic_arrays._UTest)[2]).type == Data_C_UTest_type__float);
    VERIFY(*((float*)((((Data_C_UTest*)entry->entries_dynamic_arrays._UTest)[2]).ptr)) == 4.0f);

    // A bunch of static arrays of types
    VERIFY(entry->entries_static_arrays._bool[0] == true);
    VERIFY(entry->entries_static_arrays._bool[1] == false);
    VERIFY(entry->entries_static_arrays._uint8[0] == 4);
    VERIFY(entry->entries_static_arrays._uint8[1] == 3);
    VERIFY(entry->entries_static_arrays._sint8[0] == 8);
    VERIFY(entry->entries_static_arrays._sint8[1] == 6);
    VERIFY(entry->entries_static_arrays._uint16[0] == 1);
    VERIFY(entry->entries_static_arrays._uint16[1] == 2);
    VERIFY(entry->entries_static_arrays._sint16[0] == 2);
    VERIFY(entry->entries_static_arrays._sint16[1] == 4);
    VERIFY(entry->entries_static_arrays._uint32[0] == 3);
    VERIFY(entry->entries_static_arrays._uint32[1] == 1);
    VERIFY(entry->entries_static_arrays._sint32[0] == 5);
    VERIFY(entry->entries_static_arrays._sint32[1] == 1);
    VERIFY(entry->entries_static_arrays._uint64[0] == 2);
    VERIFY(entry->entries_static_arrays._uint64[1] == 4);
    VERIFY(entry->entries_static_arrays._sint64[0] == 9);
    VERIFY(entry->entries_static_arrays._sint64[1] == 8);
    VERIFY(entry->entries_static_arrays._float[0] == 6.0f);
    VERIFY(entry->entries_static_arrays._float[1] == 7.0f);
    VERIFY(entry->entries_static_arrays._double[0] == 11.0f);
    VERIFY(entry->entries_static_arrays._double[1] == 41.0f);
    VERIFY(!strcmp((const char*)entry->entries_static_arrays._string[0], "un"));
    VERIFY(!strcmp((const char*)entry->entries_static_arrays._string[1], "deux"));
    VERIFY(entry->entries_static_arrays._color[0] == Data_C_Color_Hazel);
    VERIFY(entry->entries_static_arrays._color[1] == Data_C_Color_Blue);
    VERIFY(entry->entries_static_arrays._pos[0].x == 4.0f);
    VERIFY(entry->entries_static_arrays._pos[0].y == 3.0f);
    VERIFY(entry->entries_static_arrays._pos[1].x == 5.0f);
    VERIFY(entry->entries_static_arrays._pos[1].y == 9.0f);
    VERIFY((const Data_C_People*)((uint64_t*)entry->entries_static_arrays._people[0]) == NULL);
    VERIFY((const Data_C_People*)((uint64_t*)entry->entries_static_arrays._people[1]) != NULL);
    VERIFY(!strcmp((const char*)((const Data_C_People*)((uint64_t*)entry->entries_static_arrays._people[1]))->name, "Curly"));
    VERIFY(entry->entries_static_arrays._UTest[0].type == Data_C_UTest_type__float);
    VERIFY(*(float*)entry->entries_static_arrays._UTest[0].ptr == 99.0f);
    VERIFY(entry->entries_static_arrays._UTest[1].type == Data_C_UTest_type_None);

    Data_C_DestroyDatabase(&data);

    return true;
}

bool DoTest_Data_No_Obfuscation(const char* fileName)
{
    Data_No_Obfuscation_C_Database data;
    Data_No_Obfuscation_C_InitDatabase(&data);

    if (!Data_No_Obfuscation_C_LoadFromFile(fileName, &data))
    {
        printf("Could not load %s\n", fileName);
        return 1;
    }

    printf("%s\n", fileName);

    VERIFY(data.table_Entry_count == 1);
    const Data_No_Obfuscation_C_Entry* entries = (const Data_No_Obfuscation_C_Entry*)data.table_Entry;

    const Data_No_Obfuscation_C_Entry* entry = &entries[0];

    // A bunch of types
    VERIFY(entry->entries._bool == true);
    VERIFY(entry->entries._uint8 == 2);
    VERIFY(entry->entries._sint8 == -3);
    VERIFY(entry->entries._uint16 == 4);
    VERIFY(entry->entries._sint16 == -5);
    VERIFY(entry->entries._uint32 == 6);
    VERIFY(entry->entries._sint32 == -7);
    VERIFY(entry->entries._uint64 == 8);
    VERIFY(entry->entries._sint64 == -9);
    VERIFY(entry->entries._float == 10.0f);
    VERIFY(entry->entries._double == -11.0);
    VERIFY(!strcmp((const char*)entry->entries._string, "some text!"));
    VERIFY(entry->entries._color == Data_No_Obfuscation_C_Color_Blue);
    VERIFY(entry->entries._pos.x == 2.0f);
    VERIFY(entry->entries._pos.y == 1.0f);
    VERIFY((const Data_No_Obfuscation_C_People*)(entry->entries._people) == NULL);
    VERIFY(entry->entries._UTest.type == Data_No_Obfuscation_C_UTest_type__float);
    VERIFY(*(float*)entry->entries._UTest.ptr == 23.0f);

    // A bunch of types which have default values set in the def file
    VERIFY(entry->entries_defaults._bool == true);
    VERIFY(entry->entries_defaults._uint8 == 1);
    VERIFY(entry->entries_defaults._sint8 == -2);
    VERIFY(entry->entries_defaults._uint16 == 3);
    VERIFY(entry->entries_defaults._sint16 == -4);
    VERIFY(entry->entries_defaults._uint32 == 5);
    VERIFY(entry->entries_defaults._sint32 == -6);
    VERIFY(entry->entries_defaults._uint64 == 7);
    VERIFY(entry->entries_defaults._sint64 == -8);
    VERIFY(entry->entries_defaults._float == 10000.0f);
    VERIFY(entry->entries_defaults._double == 1000.0);
    VERIFY(!strcmp((const char*)entry->entries_defaults._string, "yo!"));
    VERIFY(entry->entries_defaults._color == Data_No_Obfuscation_C_Color_Hazel);
    VERIFY((const Data_No_Obfuscation_C_People*)(entry->entries_defaults._people) != NULL);
    VERIFY(!strcmp((const char*)((const Data_No_Obfuscation_C_People*)(entry->entries_defaults._people))->name, "Larry"));

    // A bunch of dynamic arrays of types
    VERIFY(entry->entries_dynamic_arrays._bool_count == 3);
    VERIFY(((uint8_t*)entry->entries_dynamic_arrays._bool)[0] == false);
    VERIFY(((uint8_t*)entry->entries_dynamic_arrays._bool)[1] == true);
    VERIFY(((uint8_t*)entry->entries_dynamic_arrays._bool)[2] == false);
    VERIFY(entry->entries_dynamic_arrays._uint8_count == 1);
    VERIFY(((uint8_t*)entry->entries_dynamic_arrays._uint8)[0] == 2);
    VERIFY(entry->entries_dynamic_arrays._sint8_count == 1);
    VERIFY(((int8_t*)entry->entries_dynamic_arrays._sint8)[0] == 4);
    VERIFY(entry->entries_dynamic_arrays._uint16_count == 1);
    VERIFY(((uint16_t*)entry->entries_dynamic_arrays._uint16)[0] == 6);
    VERIFY(entry->entries_dynamic_arrays._sint16_count == 1);
    VERIFY(((int16_t*)entry->entries_dynamic_arrays._sint16)[0] == 8);
    VERIFY(entry->entries_dynamic_arrays._uint32_count == 1);
    VERIFY(((uint32_t*)entry->entries_dynamic_arrays._uint32)[0] == 10);
    VERIFY(entry->entries_dynamic_arrays._sint32_count == 1);
    VERIFY(((int32_t*)entry->entries_dynamic_arrays._sint32)[0] == 12);
    VERIFY(entry->entries_dynamic_arrays._uint64_count == 1);
    VERIFY(((uint64_t*)entry->entries_dynamic_arrays._uint64)[0] == 14);
    VERIFY(entry->entries_dynamic_arrays._sint64_count == 2);
    VERIFY(((int64_t*)entry->entries_dynamic_arrays._sint64)[0] == 16);
    VERIFY(((int64_t*)entry->entries_dynamic_arrays._sint64)[1] == 18);
    VERIFY(entry->entries_dynamic_arrays._float_count == 1);
    VERIFY(((float*)entry->entries_dynamic_arrays._float)[0] == 20.0f);
    VERIFY(entry->entries_dynamic_arrays._double_count == 1);
    VERIFY(((double*)entry->entries_dynamic_arrays._double)[0] == 22.0f);
    VERIFY(entry->entries_dynamic_arrays._string_count == 3);
    VERIFY(!strcmp((char*)(((uint64_t*)entry->entries_dynamic_arrays._string)[0]), "uno"));
    VERIFY(!strcmp((char*)(((uint64_t*)entry->entries_dynamic_arrays._string)[1]), ""));
    VERIFY(!strcmp((char*)(((uint64_t*)entry->entries_dynamic_arrays._string)[2]), "tres"));
    VERIFY(entry->entries_dynamic_arrays._color_count == 2);
    VERIFY(((uint16_t*)entry->entries_dynamic_arrays._color)[0] == Data_No_Obfuscation_C_Color_Blue);
    VERIFY(((uint16_t*)entry->entries_dynamic_arrays._color)[1] == Data_No_Obfuscation_C_Color_Green);
    VERIFY(entry->entries_dynamic_arrays._pos_count == 2);
    VERIFY(((Data_No_Obfuscation_C_Pos*)entry->entries_dynamic_arrays._pos)[0].x == 96.0f);
    VERIFY(((Data_No_Obfuscation_C_Pos*)entry->entries_dynamic_arrays._pos)[0].y == 48.0f);
    VERIFY(((Data_No_Obfuscation_C_Pos*)entry->entries_dynamic_arrays._pos)[1].x == 7.0f);
    VERIFY(((Data_No_Obfuscation_C_Pos*)entry->entries_dynamic_arrays._pos)[1].y == 12.0f);
    VERIFY(entry->entries_dynamic_arrays._people_count == 3);
    VERIFY((const Data_No_Obfuscation_C_People*)(((uint64_t*)entry->entries_dynamic_arrays._people)[0]) != NULL);
    VERIFY(!strcmp((const char*)((const Data_No_Obfuscation_C_People*)(((uint64_t*)entry->entries_dynamic_arrays._people)[0]))->name, "Larry"));
    VERIFY((const Data_No_Obfuscation_C_People*)(((uint64_t*)entry->entries_dynamic_arrays._people)[1]) == NULL);
    VERIFY((const Data_No_Obfuscation_C_People*)(((uint64_t*)entry->entries_dynamic_arrays._people)[2]) != NULL);
    VERIFY(!strcmp((const char*)((const Data_No_Obfuscation_C_People*)(((uint64_t*)entry->entries_dynamic_arrays._people)[2]))->name, "Moe"));
    VERIFY(entry->entries_dynamic_arrays._UTest_count == 3);
    VERIFY((((Data_No_Obfuscation_C_UTest*)entry->entries_dynamic_arrays._UTest)[0]).type == Data_No_Obfuscation_C_UTest_type_None);
    VERIFY((((Data_No_Obfuscation_C_UTest*)entry->entries_dynamic_arrays._UTest)[1]).type == Data_No_Obfuscation_C_UTest_type__pos);
    VERIFY(((Data_No_Obfuscation_C_Pos*)((((Data_No_Obfuscation_C_UTest*)entry->entries_dynamic_arrays._UTest)[1]).ptr))->x == 2.0f);
    VERIFY(((Data_No_Obfuscation_C_Pos*)((((Data_No_Obfuscation_C_UTest*)entry->entries_dynamic_arrays._UTest)[1]).ptr))->y == 3.0f);
    VERIFY((((Data_No_Obfuscation_C_UTest*)entry->entries_dynamic_arrays._UTest)[2]).type == Data_No_Obfuscation_C_UTest_type__float);
    VERIFY(*((float*)((((Data_No_Obfuscation_C_UTest*)entry->entries_dynamic_arrays._UTest)[2]).ptr)) == 4.0f);

    // A bunch of static arrays of types
    VERIFY(entry->entries_static_arrays._bool[0] == true);
    VERIFY(entry->entries_static_arrays._bool[1] == false);
    VERIFY(entry->entries_static_arrays._uint8[0] == 4);
    VERIFY(entry->entries_static_arrays._uint8[1] == 3);
    VERIFY(entry->entries_static_arrays._sint8[0] == 8);
    VERIFY(entry->entries_static_arrays._sint8[1] == 6);
    VERIFY(entry->entries_static_arrays._uint16[0] == 1);
    VERIFY(entry->entries_static_arrays._uint16[1] == 2);
    VERIFY(entry->entries_static_arrays._sint16[0] == 2);
    VERIFY(entry->entries_static_arrays._sint16[1] == 4);
    VERIFY(entry->entries_static_arrays._uint32[0] == 3);
    VERIFY(entry->entries_static_arrays._uint32[1] == 1);
    VERIFY(entry->entries_static_arrays._sint32[0] == 5);
    VERIFY(entry->entries_static_arrays._sint32[1] == 1);
    VERIFY(entry->entries_static_arrays._uint64[0] == 2);
    VERIFY(entry->entries_static_arrays._uint64[1] == 4);
    VERIFY(entry->entries_static_arrays._sint64[0] == 9);
    VERIFY(entry->entries_static_arrays._sint64[1] == 8);
    VERIFY(entry->entries_static_arrays._float[0] == 6.0f);
    VERIFY(entry->entries_static_arrays._float[1] == 7.0f);
    VERIFY(entry->entries_static_arrays._double[0] == 11.0f);
    VERIFY(entry->entries_static_arrays._double[1] == 41.0f);
    VERIFY(!strcmp((const char*)entry->entries_static_arrays._string[0], "un"));
    VERIFY(!strcmp((const char*)entry->entries_static_arrays._string[1], "deux"));
    VERIFY(entry->entries_static_arrays._color[0] == Data_No_Obfuscation_C_Color_Hazel);
    VERIFY(entry->entries_static_arrays._color[1] == Data_No_Obfuscation_C_Color_Blue);
    VERIFY(entry->entries_static_arrays._pos[0].x == 4.0f);
    VERIFY(entry->entries_static_arrays._pos[0].y == 3.0f);
    VERIFY(entry->entries_static_arrays._pos[1].x == 5.0f);
    VERIFY(entry->entries_static_arrays._pos[1].y == 9.0f);
    VERIFY((const Data_No_Obfuscation_C_People*)((uint64_t*)entry->entries_static_arrays._people[0]) == NULL);
    VERIFY((const Data_No_Obfuscation_C_People*)((uint64_t*)entry->entries_static_arrays._people[1]) != NULL);
    VERIFY(!strcmp((const char*)((const Data_No_Obfuscation_C_People*)((uint64_t*)entry->entries_static_arrays._people[1]))->name, "Curly"));
    VERIFY(entry->entries_static_arrays._UTest[0].type == Data_No_Obfuscation_C_UTest_type__float);
    VERIFY(*(float*)entry->entries_static_arrays._UTest[0].ptr == 99.0f);
    VERIFY(entry->entries_static_arrays._UTest[1].type == Data_No_Obfuscation_C_UTest_type_None);

    Data_No_Obfuscation_C_DestroyDatabase(&data);

    return true;
}

int main(int argc, char** argv)
{
    bool success =
        DoTest_Data("packed/Data.bin") &&
        DoTest_Data_No_Obfuscation("packed/Data_No_Obfuscation.bin")
        ;

    if (!success)
        return 1;

    printf("All checks passed!\n");
    return 0;
}
