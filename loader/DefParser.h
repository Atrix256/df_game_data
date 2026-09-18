#pragma once

#include <string>
#include <vector>
#include <memory>
#include <sstream>


enum TokenType
{
    end_of_file,
    unknown,
    identifier,
    struct_def,
    type_name,
    directive_root,
    brace_begin,
    brace_end,
    bracket_begin,
    bracket_end,
    semicolon,
    equals,
};

struct Token
{
    TokenType type;
    std::string_view token;
};

class DefParser
{
public:

    enum FieldType
    {
        _uint8,
        _sint8,
        _uint16,
        _sint16,
        _uint32,
        _sint32,
        _uint64,
        _sint64,
        _float,
        _double,
        _enum,
        _struct
    };

    struct TypeField
    {
        std::string name;
        FieldType fieldType;
        bool isArray = false;
        int fixedArraySize = 0;
        std::string structName;
        std::string enumName;

        // TODO: no, enum values are listed elsewhere!
        std::vector<std::string> enumValues;
    };

    struct Type
    {
        std::string name;
        std::string nameSpace;
        std::vector<TypeField> fields;
    };

    bool Parse(const char* fileName);

    const Type* GetTypeByName(const char* name) const
    {
        for (const Type& type : m_types)
        {
            if (name == type.name)
                return &type;
        }
        return nullptr;
    }

    const char* GetRootType() const { return m_rootType.c_str(); }

    std::string GetErrorText() const { return m_errorText.str(); }

private:
    bool ParseStructDef(const char*& cursor);
    bool ParseDirectiveRoot(const char*& cursor);

    bool TokenTypeExpected(const Token& token, TokenType expectedType);

private:

    std::ostringstream m_errorText;
    std::string m_currentNamespace;

    std::string m_path;
    std::string m_rootType;
    std::vector<Type> m_types;
};
