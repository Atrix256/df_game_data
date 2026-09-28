# Data Compilation

You can compile the data in the editor under the Compile menu.

You can also compile the data through the editor's command line interface.  Compiling through the command line interface will cause it to compile data without creating a window, and will exit when done, with an exit code of 0 on success.

`Editor.exe <filename>` - Opens the editor and automatically loads the specified filename

`Editor.exe --compile <filename>` - Loads the specified filename and compiles, without creating a window.

`Editor.exe -c <filename>` - same

## Compilation Output

When data is compiled, it generates a .bin file and a .h file.

The .bin file is the binary representation of your data, written in such a way that it is read in with a single fread, and pointers are converted from file offsets to actual memory pointers.

As part of the loading process, it also makes sure that the schema hash stored in the .bin file matches the schema hash that the header was generated for.  Adding, removing or modifying data records doesn't change the schema, so doing those thigns and recompiling will result in a different bin file, but the same generated header file.  Some compilation settings can affect the schema hash though, such as the entry look up table, or the hot reloading feature, since that modifies what is stored in the .bin file.

The loading process also uses a fourcc to know if it needs to do endian swapping before pointer fixup.

The header file made can be thought of as a "header only library" which loads the binary file.  Changing the schema will change this file, but modifying data (including adding or removing data records) will not change this file.

## Header File Interface

### Loading
There are two functions for loading a .bin database file:

* `bool LoadFromMemory(void* mem, uint32_t memSize)` - loads a .bin file from memory.

* `bool LoadFromFile(const char* fileName)` - loads a .bin file from disk.

### Getting Data Entries
There are functions for getting data from tables as well.  If the table was called "Item", the functions would be named:

* `uint32_t GetItemCount()` - Says how many entries there are in the table
* `ItemRecord GetItem(uint32_t index)` - Gets an entry by index.
* `ItemRecord GetItem(const char* name)` - Gets an entry by name. The look up table option must be enabled, otherwise this function won't exist in the header.

There are templated versions of those functions as well, which can be helpful when writing generic code:

* `template <typename T> uint32_t GetCount()`

* `template <typename T> Record<T> Get(uint32_t index)`

* `template <typename T> Record<T> Get(const char* name)`

### Record Objects and Hot Reloading

Record objects hold a pointer to a data table entry and are templated to know what data type the entry is. They have a `.Get()` function on them which returns a const reference to the data entry. If the index or name was invalid, the Get() function will return a const reference to a default initialized object.  You can also call `.Valid()` to see if the record points at a valid entry or not.

To enable hot reloading, call `Tick()` on the data periodically, such as once a frame, or maybe once a second.

Record objects will automatically handle hot reload, by looking for the record with the same name in the newly loaded .bin file.  If the record name no longer exists, it will happily decay to an invalid record, where Valid() returns false, and Get() returns a default initialized object.

Any data you have cached off from a record will not be updated after a hot reload though. To help handle this, Tick will return true when hot reloading happened, so that you can react appropriately.

### Links

Links show up as pointers in the generated header.  Pointers in the header are not naked pointers though, because there needs to be enough space for either 32 or 64 bit pointers, depending on whether you are loading the data in a 32 or 64 bit program.  The pointer type is a templated type named `Ptr64`, so a link to a `Monster` would end up being a `Ptr64<Monster>` in the struct. You use the `.ptr` field to actually get the `Monster*` pointer.  If a link was left blank in the data, this pointer will be null.

```cpp
    template <typename T>
    union Ptr64
    {
        T* ptr;
        uint64_t _64 = 0;
    };
```

### Unions

Unions work differently than you might expect.  Instead of declaring a union in the generated header, it has an enum which says which field is used, and then a pointer to that fields data.  Instead of serializing a C++ style union, which would require as much memory as the largest member in the union, this only stores the actual member used.  If the union is set the "none", the pointer to the field data is null. 

Here is the .def description of an item, which includes a union for the item type:

```cpp
// items.def
struct Weapon
{
    float damage = 0.1;
};

struct Armor
{
    float armor = 0.5;
};

struct Consumable
{
    float hpGain = 0.0;
    float mpGain = 0.0;
};

union ItemBase
{
    Weapon weapon;
    Armor armor;
    float luck = 3.2;
    Consumable consumable;
};

struct Item
{
    string name;
    ItemBase item;
};

#root Item
```

Here is the generated code:

```cpp
    struct Weapon
    {
        float damage;
    };

    struct Armor
    {
        float armor;
    };

    struct Consumable
    {
        float hpGain;
        float mpGain;
    };

    enum class ItemBase_type : uint16_t
    {
        None,
        weapon,
        armor,
        luck,
        consumable,
    };

    struct ItemBase
    {
        ItemBase_type type;
        Ptr64<void> ptr;

        using weapon_type = Weapon;
        using armor_type = Armor;
        using luck_type = float;
        using consumable_type = Consumable;

        weapon_type* weapon() { return type == ItemBase_type::weapon ? reinterpret_cast<weapon_type*>(ptr.ptr) : nullptr; }
        const weapon_type* weapon() const { return type == ItemBase_type::weapon ? reinterpret_cast<const weapon_type*>(ptr.ptr) : nullptr; }

        armor_type* armor() { return type == ItemBase_type::armor ? reinterpret_cast<armor_type*>(ptr.ptr) : nullptr; }
        const armor_type* armor() const { return type == ItemBase_type::armor ? reinterpret_cast<const armor_type*>(ptr.ptr) : nullptr; }

        luck_type* luck() { return type == ItemBase_type::luck ? reinterpret_cast<luck_type*>(ptr.ptr) : nullptr; }
        const luck_type* luck() const { return type == ItemBase_type::luck ? reinterpret_cast<const luck_type*>(ptr.ptr) : nullptr; }

        consumable_type* consumable() { return type == ItemBase_type::consumable ? reinterpret_cast<consumable_type*>(ptr.ptr) : nullptr; }
        const consumable_type* consumable() const { return type == ItemBase_type::consumable ? reinterpret_cast<const consumable_type*>(ptr.ptr) : nullptr; }
    };

    struct Item
    {
        Ptr64<char> name;
        ItemBase item;
    };

    using ItemRecord = Record<Item>;
```

As you can see, the union type `ItemBase` becomes an enum and a pointer.  The generated code for the union also has some helper functions named after the fields of the union.  You can call them to get a pointer to that field, or a null if the union is not storing that field.

### Arrays

Arrays come in two flavors: static sized arrays and dynamic sized arrays.  Both type of arrays have a uint32 named "<fieldName>_count" which says how many items there are in the array, and then the array comes after that.

For static arrays, the count is a compile time constant, and the array is a normal C style array there in the generated header struct.

For dynamic arrays, the count is read from the bin file, then there is a pointer (Ptr64) to the data.

Here is a snippet of a def file defining a static array and a dynamic array:

```cpp
// ...
uint16 numbers_fixed[4];
uint16 numbers_dynamic[];
// ...
```

Here is the generated code:
```cpp
//...
        static const uint32_t numbers_fixed_count = 4;
        uint16_t numbers_fixed[4];
        uint32_t numbers_dynamic_count = 0;
        Ptr64<uint16_t> numbers_dynamic;
//...
```
