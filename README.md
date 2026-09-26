# df_game_data
Minimalist C++ game data middleware by Alan Wolfe.

Describe schemas, edit data, binary pack data, load data with a generated header. Hot reloading support.

## Data Model

A database is made up of tables.

A table is an array of entries that can be looked up by name or index.

All entries in a table use the same struct.

struct fields can be:
* bool - true or false
* integers - uint8, int8, uint16, int16, uint32, int32, uint64, int64
* floats - float, double
* string - text
* enum - integers that have labels
* struct - a different struct
* union - a data type that allows a field to be one of a specific list of types. Can be null.
* link - a link to a table entry. Can be null.

Struct fields may also be dynamic or static sized arrays of the above types.

## Use Overview

The process for using df_game_data is:
1. Define A Table
2. Edit Data
3. Compile Database And Use Output

### 1. Define A Table

[Full .def Spec](def.md)

Make a .def file to define a table schema.  This is a C/C++ style definition of types.

```cpp
// Monster.def
#include "common.def"  // defines Vec3 struct

enum Alignment
{
    Good,
    Evil,
    Neutral
};

struct Monster
{
    string name;
    float hp = 20.0;
    Vec3 spawnPosition;
    Alignment alignment = Good;
};

// This table will have entries of the Monster struct
#root Monster
```

### 2. Edit Data

[Editor Details](editor.md)

If you open your .def file in the editor, it will create a .dbroot file automatically.

You can add entries to your table, and you can add other tables to your database.

Each data entry is a .json file on disk, in the same folder as the .def file. In general, you want one folder per .def file.

The data entries are separate files to minimize merge conflicts when merging development branches.

![The editor](editor.png)

### 3. Compile Database And Use Output

[Compiled Output Details](output.md)

Compilation settings can be found under the **Edit** menu, and allows you to configure multiple build targets with different compilation options.

You can compile the database using the **Compile** menu, or by pressing CTRL+C.

Compilation creates a .bin file which is the binary representation of all entries in all tables in your .dbroot file.

It also creates a .h file.  This is a standalone header file which you include in your code to load the .bin file and read data from it.

```cpp
// main.cpp
#include "data.h"

int main(int argc, char** argv)
{
    Data data;
    if (!data.LoadFromFile("data.bin"))
        return 1;

    Data::MonsterRecord kobold = data.GetMonster("kobold");

    uint32_t monsterCount = data.GetMonsterCount();

    Data::MonsterRecord otherMonster = data.GetMonster(3);

    while(!GameOver())
    {
        PlayGame();

        // Call Tick() for hot reloading.
        // Records auto update after hot reload, but any data you cached from a record
        // will need to be updated manually.
        // Tick returns true when a hot reload happens, so that you can react to it.
        data.Tick();
    }

    return 0;
}
```

## Building

This was developed on windows using microsoft visual studio.  Cloning the repo and building the solution should be all that is needed.

## Contributors

Created by Alan Wolfe

<First contributor will go here!>

### Contributing

Contributions are most welcome!

Please try to follow the style of code near where you are editing, and test your changes.

You can add your name to the contributor list in the section above (sorted alphabetically by first name). Any change large
or small earns a spot in the contributor list.

If you are looking for ideas to contribute, the issues list has several todo items.

Regarding AI use - If you have a well formed contribution, I have know way of knowing if you used AI or not unless you tell me. Low quality submissions will be met with guidance. Large volumes of low quality submissions will be met with silence.

## Open Sourced Software Used

Thank you to the creators of the following!

| Software | Use | URL |
| -- | -- | -- |
| rapidjson | To load json data | https://rapidjson.org/ |
| Dear ImGui | For editor UI | https://github.com/ocornut/imgui |
| Font Awesome | Editor button icons | https://github.com/FortAwesome/Font-Awesome |
| xxHash | Portable non crypto hash | https://github.com/cyan4973/xxhash |
