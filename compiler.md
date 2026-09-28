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

Record objects hold a pointer to a data table entry and are templated to know what data type the entry is. They have a `.Get()` function on them which returns a const reference to the data entry. If the index or name was invalid, the Get() function will return a const reference to a default initialized object.

To enable hot reloading, call `Tick()` on the data periodically, such as once a frame, or maybe once a second.

Record objects will automatically handle hot reload, by looking for the record with the same name in the newly loaded .bin file.

However, any data you have cached off from a record will not be updated. To help handle this, Tick will return true when hot reloading happened, so that you can react appropriately.
