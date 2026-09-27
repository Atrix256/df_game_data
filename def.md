# .def File Format

The .def file format will look familiar to C/C++ programmers, with minimal differences.

### Includes

define files can include other def files using `#include`:

```cpp
#include "../path/file.def"
```

### Namespaces

You can set a namespace, which will put all types after it into the namespace. This can help
naming collisions across different files. The next namespace statement will set the new namespace,
it doesn't nest them. Include files do not inherit the namespace they are included in.

```cpp
namespace TestData;
```

### Structs

You define a struct with the struct keyword, like you do in C++.

```cpp
struct SomeStruct
{
    float a;
    int b;
};
```

### Unions

Unions are declared similarly, but can only store one of the fields. Unions are a way of
having a type which can be one of many other types.  For instance, you could have a
list of components on an object, where each component was a union of the different component
types. That would let you have a list of different types of components on the object.  Unions can
also be "no object", so can be null.

```cpp
union SomeUnion
{
    float a;
    int b;
};
```

### Enums

Enums are integer values, but they have a human readable label to use instead of a "magic number".

```cpp
enum Vegetables
{
    Carrot,
    Zucchini,
    Broccoli
};
```

### Table Root Type

You must set the root type of a table using the `#root` keyword.  The root type must be a struct.
```cpp
#root SomeStruct
```

### Comments

Both single line comments, and block comments are supported.

```cpp
// This is my favorite struct
struct MyStruct
{
    float health;
    //float mana;
    int experience;

/*
    float armor;
    float attack;
    float flamability;
*/

    string favoriteSnack;
};

/* Here is an explanation
why this is my
favorite struct.
It just is.*/

```

## Field Types

Fields can be various types:

* bool - true or false
* integers - uint8, int8, uint16, int16, uint32, int32, uint64, int64
* floats - float, double
* string - text
* enum - integers that have labels
* struct - a different struct
* union - a data type that allows a field to be one of a specific list of types. Can be null.
* link - a link to a table entry. Can be null.

default values can optionally be specified. If no default given, the field is zero initialized.

```cpp
struct SomeStruct
{
    int a = 3;
    float b = 1.34f;
    int c; // zero initialized
    string s = "hello!";
};
```

### Links

The only new one here is link.  A link can point at entries from this table, or other tables. A link can also be none (null).  A link is a templated object which takes the table root type as the parameter.

```cpp
struct DoubleLink
{
    Link<SomeStruct> link1;
    Link<Monster> link2;
};
```

### Arrays

Any struct field can be an array. Arrays can either be fixed size or dynamic size. A fixed size array
gives the size in brackets, while a dynamic array does not give a size.

```cpp
struct ArrayStruct
{
    // A float3 position
    float position[3];

    // An unbounded number of fears of specific monsters.
    Link<Monster> fears[];
};
```
