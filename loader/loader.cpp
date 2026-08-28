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

static inline std::string DocumentationToString(const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>* doc)
{
    std::string ret;
    if (!doc)
        return ret;

    for (size_t i = 0; i < doc->size(); ++i)
    {
        if (i > 0)
            ret += '\n';
        ret += doc->Get(i)->str();
    }
    return ret;
}

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
            newEnum.comment = DocumentationToString(e->documentation());

            // Loop through the values in the enum
            for (flatbuffers::uoffset_t j = 0; j < e->values()->size(); ++j)
            {
                const reflection::EnumVal* val = e->values()->Get(j);
                if (!val)
                    continue;

                DBEnumItem& newItem = newEnum.items.emplace_back();
                newItem.name = val->name()->str();
                newItem.value = val->value();
                newItem.comment = DocumentationToString(val->documentation());

                const reflection::Type* unionType = val->union_type();
                if (!unionType)
                    continue;

                FromFlatBufferType(*unionType, newItem.unionBaseType, newItem.unionElementType);
            }
        }
    }

    // Get the objects
    {
        bool foundRoot = false;
        auto objects = schema.objects();
        for (flatbuffers::uoffset_t i = 0; i < objects->size(); ++i)
        {
            const reflection::Object* obj = objects->Get(i);
            if (!obj)
                continue;

            DBObject& newObj = m_objects.emplace_back();
            newObj.name = obj->name()->str();
            newObj.comment = DocumentationToString(obj->documentation());

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
                newField.comment = DocumentationToString(field->documentation());
                newObj.fields.push_back(newField);
            }

            if (!obj->attributes())
                continue;

            // Remember if this is a root_type object
            for (flatbuffers::uoffset_t j = 0; j < obj->attributes()->size(); ++j)
            {
                auto attr = obj->attributes()->Get(j);
                if (attr && attr->key() && attr->key()->str() == "root_type")
                {
                    if (foundRoot)
                    {
                        m_errorText = "Multiple root types found in file: " + std::string(path);
                        return false;
                    }
                    newObj.isRoot = true;
                    foundRoot = true;
                }
            }
        }

        if (!foundRoot)
        {
            m_errorText = "No root type found in file: " + std::string(path) + "\nPlease use the (root_type) attribute to designate a root type.";
            return false;
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
