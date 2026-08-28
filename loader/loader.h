#pragma once

#include <vector>
#include <string>

struct DBObjectFieldType
{
    DBObjectFieldType()
    {
    }

    enum class Type
    {
        Bool,
        Int,
        Float,
        String,
        Vector,
        Object,
        Union,

        Unknown
    };
    Type type = Type::Unknown;

    union
    {
        struct Int_
        {
            bool isSigned = false;
            uint8_t bytes = 0;
            bool isUnionDiscriminant = false;
            int enumIndex = -1;
        };
        Int_ Int;

        struct Float_
        {
            bool isDouble = false;
        };
        Float_ Float;

        struct Vector_
        {
            // 0 for dynamic (vector). all else is a fixed size (array).
            size_t fixedSize = 0;
        };
        Vector_ Vector;

        struct Object_
        {
            int objectIndex = -1;
        };
        Object_ Object;

        struct Union_
        {
            int enumIndex = -1;
        };
        Union_ Union;
    };
};

struct DBObjectField
{
    std::string name;
    std::string comment;
    DBObjectFieldType baseType;
    DBObjectFieldType elementType;
};

struct DBObject
{
    std::string name;
    std::string comment;
    std::vector<DBObjectField> fields;
    bool isRoot = false;
};

struct DBEnumItem
{
    std::string name;
    std::string comment;
    int64_t value = 0;

    DBObjectFieldType unionBaseType;
    DBObjectFieldType unionElementType;
};

struct DBEnum
{
    std::string name;
    std::string comment;
    std::vector<DBEnumItem> items;
    bool isUnion = false;
};

class DBTable
{
public:
    bool Load(const char* path);

    const char* GetPath() const
    {
        return m_path.c_str();
    }

    const char* GetErrorText() const
    {
        return m_errorText.c_str();
    }

    const std::vector<DBEnum>& GetEnums() const
    {
        return m_enums;
    }

private:
    bool EnsureBFBSExists();

private:
    std::string m_path;
    std::string m_errorText;

    std::vector<DBEnum> m_enums;
    std::vector<DBObject> m_objects;
};

class DBRoot
{
public:
    bool Load(const char* path);

    void Clear();

    const std::vector<DBTable>& GetTables() const
    {
        return m_tables;
    }

    const char* GetErrorText() const
    {
        return m_errorText.c_str();
    }

private:
    std::vector<DBTable> m_tables;
    std::string m_errorText;
};
