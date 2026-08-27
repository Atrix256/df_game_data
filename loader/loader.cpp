#include "loader.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <filesystem>

#include "flatbuffers/flatbuffers.h"
#include "flatbuffers/idl.h"
#include "flatbuffers/util.h"
#include "flatbuffers/reflection.h"

#include "LaunchProcess.h"

static inline DBObjectFieldType FromFlatBufferType(const reflection::BaseType& type)
{
    DBObjectFieldType ret;

    static_assert(reflection::BaseType::MaxBaseType == 19);
    switch (type)
    {
        case reflection::BaseType::None:
        {
            break;
        }
        case reflection::BaseType::UType:
        {
            ret.type = DBObjectFieldType::Type::Int;
            ret.Int.isSigned = false;
            ret.Int.bytes = 4;
            ret.Int.isUnionDiscriminant = true;
            break;
        }
        case reflection::BaseType::Bool:
        {
            ret.type = DBObjectFieldType::Type::Bool;
            break;
        }
        case reflection::BaseType::Byte:
        {
            ret.type = DBObjectFieldType::Type::Int;
            ret.Int.isSigned = true;
            ret.Int.bytes = 1;
            ret.Int.isUnionDiscriminant = false;
            break;
        }
        case reflection::BaseType::UByte:
        {
            ret.type = DBObjectFieldType::Type::Int;
            ret.Int.isSigned = false;
            ret.Int.bytes = 1;
            ret.Int.isUnionDiscriminant = false;
            break;
        }
        case reflection::BaseType::Short:
        {
            ret.type = DBObjectFieldType::Type::Int;
            ret.Int.isSigned = true;
            ret.Int.bytes = 2;
            ret.Int.isUnionDiscriminant = false;
            break;
        }
        case reflection::BaseType::UShort:
        {
            ret.type = DBObjectFieldType::Type::Int;
            ret.Int.isSigned = false;
            ret.Int.bytes = 2;
            ret.Int.isUnionDiscriminant = false;
            break;
        }
        case reflection::BaseType::Int:
        {
            ret.type = DBObjectFieldType::Type::Int;
            ret.Int.isSigned = true;
            ret.Int.bytes = 4;
            ret.Int.isUnionDiscriminant = false;
            break;
        }
        case reflection::BaseType::UInt:
        {
            ret.type = DBObjectFieldType::Type::Int;
            ret.Int.isSigned = false;
            ret.Int.bytes = 4;
            ret.Int.isUnionDiscriminant = false;
            break;
        }
        case reflection::BaseType::Long:
        {
            ret.type = DBObjectFieldType::Type::Int;
            ret.Int.isSigned = true;
            ret.Int.bytes = 8;
            ret.Int.isUnionDiscriminant = false;
            break;
        }
        case reflection::BaseType::ULong:
        {
            ret.type = DBObjectFieldType::Type::Int;
            ret.Int.isSigned = false;
            ret.Int.bytes = 8;
            ret.Int.isUnionDiscriminant = false;
            break;
        }
        case reflection::BaseType::Float:
        {
            ret.type = DBObjectFieldType::Type::Float;
            ret.Float.isDouble = false;
            break;
        }
        case reflection::BaseType::Double:
        {
            ret.type = DBObjectFieldType::Type::Float;
            ret.Float.isDouble = true;
            break;
        }
        case reflection::BaseType::String:
        {
            ret.type = DBObjectFieldType::Type::String;
            break;
        }
        case reflection::BaseType::Vector:
        {
            ret.type = DBObjectFieldType::Type::Vector;
            ret.Vector.fixedSize = 0;
            break;
        }
        case reflection::BaseType::Obj:
        {
            ret.type = DBObjectFieldType::Type::Object;
            break;
        }
        case reflection::BaseType::Union:
        {
            ret.type = DBObjectFieldType::Type::Union;
            break;
        }
        case reflection::BaseType::Array:
        {
            ret.type = DBObjectFieldType::Type::Vector;
            ret.Vector.fixedSize = 0;
            break;
        }
        case reflection::BaseType::Vector64:
        {
            ret.type = DBObjectFieldType::Type::Vector;
            ret.Vector.fixedSize = 0;
            break;
        }
    }

    return ret;
}

static inline void FromFlatBufferType(const reflection::Type& fieldType, DBObjectFieldType& baseType, DBObjectFieldType& elementType)
{
    // base_type is the type. if it's a vector or array, then element is the type in that vector or array.
    baseType = FromFlatBufferType(fieldType.base_type());
    switch (baseType.type)
    {
        case DBObjectFieldType::Type::Vector:
        {
            if (fieldType.base_type() == reflection::BaseType::Array)
                baseType.Vector.fixedSize = fieldType.fixed_length();
            else
                baseType.Vector.fixedSize = 0;

            elementType = FromFlatBufferType(fieldType.element());
            break;
        }
        case DBObjectFieldType::Type::Object:
        {
            baseType.Object.objectIndex = fieldType.index();
            break;
        }
        case DBObjectFieldType::Type::Int:
        {
            baseType.Int.enumIndex = fieldType.index();
            break;
        }
        case DBObjectFieldType::Type::Union:
        {
            baseType.Union.enumIndex = fieldType.index();
            break;
        }
    }
}

bool DBTable::EnsureBFBSExists()
{
    // Find out when the input file was last modified
    std::error_code ec;
    auto inFileTime = std::filesystem::last_write_time(m_path, ec);
    if (ec)
    {
        m_errorText = "Failed to get last write time for db file: " + m_path + "\nError: " + ec.message();
        return false;
    }

    // remake the .bfbs if it doesn't exist or is stale
    std::filesystem::path outFileName = std::filesystem::path(m_path).replace_extension(".bfbs");
    auto outFileTime = std::filesystem::last_write_time(outFileName, ec);
    if (ec || inFileTime > outFileTime)
    {
        std::filesystem::path outDir = std::filesystem::path(m_path).remove_filename();
        std::ostringstream command;
        command << ".\\flatc.exe -b --schema -o " << outDir << " " << m_path;
        LaunchProcessResult result = LaunchProcess(command.str().c_str());
        if (result.exitCode != 0)
        {
            m_errorText = "Failed to run flatc.exe on db file: " + m_path + "\nExit code: " + std::to_string(result.exitCode) + "\nOutput:\n" + result.output;
            return false;
        }
    }

    return true;
}

bool DBTable::Load(const char* path)
{
    m_path = path;

    // make sure the .bfbs file exists
    if (!EnsureBFBSExists())
        return false;

    // Load the binary bfbs file
    std::filesystem::path outFileName = std::filesystem::path(m_path).replace_extension(".bfbs");
    std::string bfbs_file_contents;
    bool success = flatbuffers::LoadFile(outFileName.string().c_str(), true, &bfbs_file_contents);
    if (!success)
    {
        m_errorText = "Failed to load bfbs file: " + outFileName.string();
        return false;
    }

    // Get the items from the schema
    const reflection::Schema& schema = *reflection::GetSchema(bfbs_file_contents.c_str());

    // Get the enums
    {
        auto enums = schema.enums();
        for (flatbuffers::uoffset_t i = 0; i < enums->size(); ++i)
        {
            const reflection::Enum* e = enums->Get(i);
            if (!e)
                continue;

            DBEnum& newEnum = m_enums.emplace_back();
            newEnum.name = e->name()->str();
            newEnum.isUnion = e->is_union();

            // Loop through the values in the enum
            for (flatbuffers::uoffset_t j = 0; j < e->values()->size(); ++j)
            {
                const reflection::EnumVal* val = e->values()->Get(j);
                if (!val)
                    continue;

                DBEnumItem& newItem = newEnum.items.emplace_back();
                newItem.name = val->name()->str();
                newItem.value = val->value();

                const reflection::Type* unionType = val->union_type();
                if (!unionType)
                    continue;

                FromFlatBufferType(*unionType, newItem.unionBaseType, newItem.unionElementType);
            }
        }
    }

    // Get the objects
    {
        auto objects = schema.objects();
        for (flatbuffers::uoffset_t i = 0; i < objects->size(); ++i)
        {
            const reflection::Object* obj = objects->Get(i);
            if (!obj)
                continue;

            DBObject& newObj = m_objects.emplace_back();
            newObj.name = obj->name()->str();

            // Loop through the fields in the object
            for (flatbuffers::uoffset_t j = 0; j < obj->fields()->size(); ++j)
            {
                const reflection::Field* field = obj->fields()->Get(j);
                if (!field)
                    continue;
                const reflection::Type* fieldType = field->type();
                if (!fieldType)
                    continue;

                DBObjectField newField;
                FromFlatBufferType(*fieldType, newField.baseType, newField.elementType);

                if (newField.baseType.type == DBObjectFieldType::Type::Unknown)
                    continue;

                if (newField.baseType.type == DBObjectFieldType::Type::Vector && newField.elementType.type == DBObjectFieldType::Type::Unknown)
                    continue;

                newField.name = field->name()->str();
                newObj.fields.push_back(newField);
            }
        }
    }

    // TODO: need to store off which one is the root type! error if < 1 or > 1 found
    // Find the root type by looking for a custom attribute "root_type"
    auto objects = schema.objects();
    for (flatbuffers::uoffset_t i = 0; i < objects->size(); ++i)
    {
        const reflection::Object* obj = objects->Get(i);
        if (!obj || !obj->attributes())
            continue;

        // Loop through the KeyValue attributes on this object/table
        for (flatbuffers::uoffset_t j = 0; j < obj->attributes()->size(); ++j)
        {
            auto attr = obj->attributes()->Get(j);
            if (attr && attr->key() && attr->key()->str() == "root_type")
            {
                int ijkl = 0;
            }
        }
    }

    return true;
}

bool DBRoot::Load(const char* path)
{
    Clear();

    // Load m_tables
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            m_errorText = "Failed to open dbroot file: " + std::string(path);
            return false;
        }

        std::filesystem::path base_path = std::filesystem::absolute(path).remove_filename();

        std::string line;
        while (std::getline(file, line))
        {
            std::filesystem::path full_path = std::filesystem::weakly_canonical(base_path / line);

            DBTable& newTable = m_tables.emplace_back();
            if (!newTable.Load(full_path.string().c_str()))
            {
                m_errorText = newTable.GetErrorText();
                Clear();
                file.close();
                return false;
            }
        }

        std::sort(m_tables.begin(), m_tables.end(), [](const DBTable& a, const DBTable& b) {
            return strcmp(a.GetPath(), b.GetPath()) < 0;
        });

        file.close();
    }

    return true;
}

void DBRoot::Clear()
{
    m_tables.clear();
}

/*
TODO:
* allow multiple root types per file, or error if there are 0 or > 1?
  * multiple types would just affect the schema code gen, and the drop down menu, so that's doable
  * but what does that mean for data records?? i think 1 root type is correct.
* use "documentation" field as tooltips in editor
* how do we support schema changes? we need a resave of all the data.
 * could maybe have a "convert" option from one known type to another?
 * angel script? idk.
 * source data maybe should have schema? (object names, hash, ??)
* TODOs in this file and other files
* support singular items, so when you open the db there is one record only, not a dictionary of them.
* note that there is a (key) field in flatbuffers, which apparently sorts by that field for binary search lookup.
* make sure you support unicode file names. run everything from within a unicode path.
*/

/*
Schemas...

? how to find the root type?
 1) only allow 1 table, and it's the table (not great limiting usage)
 2) Match name of a table to the name of the db (from filename maybe? idk)
 3) Custom attribute.

Yeah, custom attribute!
Near top of file: (maybe you inject this, actually, when loading the file)
attribute "root_type";

Then:
struct Vec3 (root_type) {
  x:float;
  y:float;
  z:float;
}

Then find that custom attribute on a struct or table.
Put this in the instructions

TODO:
* make it add the "root_type" attribute automatically. Will have to move the file over to a tmp directory and work there and copy it back.
* use namespaces feature in the schema?
* use includes feature in the schema, or at least allow it and show it as part of the example code

*/

/*
TODO:
Hot reloading:
Have a record reference for each type. Those can survive a reload(*). Maybe store name internally and a load version #. It can relook up when used, and db load version is different.

Dont cache anything off from the recods (**)

* - deleting a data record makes it be default valued.
** - you can, at your own risk. But you get a callback on hot reload so can update whatever you want in response.

Have same interface for non hot reload version. Just, data records etc are simpler.
*/

/*
TODO: Example data:
* use enums
* use unions
* use various types
* use links to other DBs
*/
