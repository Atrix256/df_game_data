#pragma once

#include <string>
#include <vector>
#include <memory>
#include <sstream>


enum class TokenType : uint8_t;

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
        _bool,
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
        _string,
        _enum,
        _struct,
        _link
    };

    struct StructField
    {
        std::string name;
        FieldType fieldType;
        bool isArray = false;
        int fixedArraySize = 0;

        std::string structName;
        std::string enumName;
        std::string linkName;

        // Zero initialized if empty
        std::string dflt;
    };

    struct Struct
    {
        std::string FullName() const
        {
            if (nameSpace.empty())
                return name;
            else
                return nameSpace + "::" + name;
        }

        std::string name;
        std::string nameSpace;
        std::vector<StructField> fields;
    };

    struct Enum
    {
        std::string FullName() const
        {
            if (nameSpace.empty())
                return name;
            else
                return nameSpace + "::" + name;
        }

        bool GetLabelIndex(const char* name, size_t& index) const
        {
            index = 0;
            while (index < labels.size())
            {
                if (labels[index] == name)
                    return true;
                index++;
            }
            return false;
        }

        bool ContainsLabel(const char* name) const
        {
            size_t index;
            return GetLabelIndex(name, index);
        }

        std::string name;
        std::string nameSpace;
        std::vector<std::string> labels;
    };

    bool Parse(const char* fileName);

    const Struct* GetStructByName(const char* name) const;
    const Enum* GetEnumByName(const char* name) const;

    const Struct* GetRootStruct() const { return GetStructByName(m_rootStruct.c_str()); }

    std::string GetRootStructName() const { return m_rootStruct; }

    std::string GetErrorText() const { return m_errorText.str(); }

private:
    bool ParseStructDef(const char*& cursor);
    bool ParseEnumDef(const char*& cursor);
    bool ParseDirectiveRoot(const char*& cursor);
    bool ParseDirectiveInclude(const char*& cursor);
    bool ParseNamespace(const char*& cursor);
    bool ParseNamespacedIdentifier(const char*& cursor, Token& token);

    void GetToken(const char*& cursor, Token& token);

    bool TokenTypeExpected(const Token& token, TokenType expectedType);

    bool SkipWhiteSpaceAndNewlines(const char*& cursor);
    bool SkipWhiteSpaceAndNewlinesAndComments(const char*& cursor);
    bool SkipComments(const char*& cursor);

private:

    int m_lineNumber = 1;
    std::ostringstream m_errorText;
    std::string m_currentNamespace;

    std::string m_path;

    std::string m_rootStruct;

    std::vector<Struct> m_structs;
    std::vector<Enum> m_enums;
};
