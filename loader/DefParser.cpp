#include "DefParser.h"

#include <filesystem>
#include "loader.h"

enum class TokenType : uint8_t
{
    Unknown,
    EndOfFile,
    Identifier,

    StructDef,
    EnumDef,
    Namespace,

    DirectiveRoot,
    DirectiveInclude,

    LiteralString,
    LiteralString_Unterminated,
    LiteralBool,
    LiteralInt,
    LiteralFloat,

    BraceBegin,
    BraceEnd,
    BracketBegin,
    BracketEnd,
    Semicolon,
    Comma,
    Equals,
    LessThan,
    GreaterThan
};

static bool IsNewLine(char c)
{
    return c == '\r' || c == '\n';
}

static bool SkipWhiteSpace(const char*& cursor)
{
    bool ret = false;
    while (cursor[0] != '\n' && std::isspace(*cursor))
    {
        cursor++;
        ret = true;
    }
    return ret;
}

bool DefParser::SkipWhiteSpaceAndNewlines(const char*& cursor)
{
    bool ret = false;
    while (std::isspace(*cursor))
    {
        if (*cursor == '\n')
            m_lineNumber++;
        cursor++;
        ret = true;
    }
    return ret;
}

static bool SkipComments(const char*& cursor)
{
    if (cursor[0] != '/')
        return false;

    // single line comment
    if (cursor[1] == '/')
    {
        cursor += 2;
        while (*cursor && *cursor != '\n')
            cursor++;
        return true;
    }
    // multi line comment
    else if (cursor[1] == '*')
    {
        cursor += 2;
        while (cursor[0] && cursor[1] && !(cursor[0] == '*' && cursor[1] == '/'))
            cursor++;

        if (cursor[0] && cursor[1] && cursor[0] == '*' && cursor[1] == '/')
            cursor += 2;

        return true;
    }

    return false;
}

bool DefParser::SkipWhiteSpaceAndNewlinesAndComments(const char*& cursor)
{
    bool ret = false;
    while (true)
    {
        if (SkipWhiteSpaceAndNewlines(cursor) || SkipComments(cursor))
            ret = true;
        else
            break;
    }
    return ret;
}

static bool IsValidSymbolCharacter(char c, bool firstCharacter)
{
    if (std::isalpha(c) || c == '_')
        return true;

    if (std::isdigit(c))
        return !firstCharacter;

    return false;
}

// Converts a #identifier token into a specific identifier, if it is one
static void ConvertDirectiveToken(Token& token)
{
    struct IdentifierToTokenType
    {
        const char* identifier;
        TokenType type;
    };

    static const IdentifierToTokenType map[] =
    {
        {"#root", TokenType::DirectiveRoot},
        {"#include", TokenType::DirectiveInclude},
    };

    for (const IdentifierToTokenType& m : map)
    {
        if (token.token == m.identifier)
        {
            token.type = m.type;
            return;
        }
    }
    token.type = TokenType::Unknown;
}

// Converts a generic identifier token into a specific identifier, if it is one
static void ConvertIdentifierToken(Token& token)
{
    struct IdentifierToTokenType
    {
        const char* identifier;
        TokenType type;
    };

    static const IdentifierToTokenType map[] =
    {
        {"struct", TokenType::StructDef},
        {"enum", TokenType::EnumDef},
        {"namespace", TokenType::Namespace},
        {"false", TokenType::LiteralBool},
        {"true", TokenType::LiteralBool},
    };

    for (const IdentifierToTokenType& m : map)
    {
        if (token.token == m.identifier)
        {
            token.type = m.type;
            return;
        }
    }

    token.type = TokenType::Identifier;
}

static bool IdentifierToFieldType(const char* identifier, DefParser::FieldType& fieldType)
{
    struct Map
    {
        const char* identifier;
        DefParser::FieldType type;
    };

    static const Map map[] =
    {
        {"bool", DefParser::FieldType::_bool},
        {"uint8", DefParser::FieldType::_uint8},
        { "int8", DefParser::FieldType::_sint8},
        {"uint16", DefParser::FieldType::_uint16},
        { "int16", DefParser::FieldType::_sint16},
        {"uint32", DefParser::FieldType::_uint32},
        { "int32", DefParser::FieldType::_sint32},
        {"uint64", DefParser::FieldType::_uint64},
        { "int64", DefParser::FieldType::_sint64},
        { "float", DefParser::FieldType::_float},
        { "double", DefParser::FieldType::_double},
        { "string", DefParser::FieldType::_string},
    };

    for (const Map& m : map)
    {
        if (!strcmp(identifier, m.identifier))
        {
            fieldType = m.type;
            return true;
        }
    }
    return false;
}

void DefParser::GetToken(const char*& cursor, Token& token)
{
    SkipWhiteSpaceAndNewlinesAndComments(cursor);

    if (*cursor == 0)
    {
        token.token = std::string_view(cursor, cursor+1);
        token.type = TokenType::EndOfFile;
        return;
    }

    // an identifier
    if (IsValidSymbolCharacter(*cursor, true))
    {
        const char* start = cursor;
        cursor++;
        while (IsValidSymbolCharacter(*cursor, false))
            cursor++;

        token.token = std::string_view(start, cursor);
        token.type = TokenType::Identifier;
        ConvertIdentifierToken(token);

        return;
    }
    // a # directive
    else if (*cursor == '#')
    {
        const char* start = cursor;
        cursor++;
        while (IsValidSymbolCharacter(*cursor, false))
            cursor++;

        token.token = std::string_view(start, cursor);
        token.type = TokenType::Identifier;
        ConvertDirectiveToken(token);
        return;
    }
    // string literal
    else if (*cursor == '\"')
    {
        const char* start = cursor + 1;
        cursor++;
        while (*cursor != 0 && !IsNewLine(*cursor) && *cursor != '\"')
            cursor++;

        if (*cursor == '\"')
        {
            token.token = std::string_view(start, cursor);
            token.type = TokenType::LiteralString;
            cursor++;
            return;
        }
        else
        {
            token.token = "";
            token.type = TokenType::LiteralString_Unterminated;
            return;
        }
    }
    // a number
    else if (std::isdigit(*cursor) || *cursor=='-')
    {
        const char* start = cursor;
        cursor++;
        while (std::isdigit(*cursor))
            cursor++;

        // scientific notation for a float
        if (*cursor == 'e')
        {
            cursor++;
            while (std::isdigit(*cursor))
                cursor++;
            token.token = std::string_view(start, cursor);
            token.type = TokenType::LiteralFloat;
        }
        // decimal place for a float
        else if (*cursor == '.')
        {
            cursor++;
            while (std::isdigit(*cursor))
                cursor++;
            token.token = std::string_view(start, cursor);
            token.type = TokenType::LiteralFloat;
        }
        // else just an integer
        else
        {
            token.token = std::string_view(start, cursor);
            token.type = TokenType::LiteralInt;
        }

        return;
    }
    else
    {
        // TODO: look for...
        // integer literals
        // float literals
        // for enums, they will want an identifer
    }

    // TODO: error if a block quote reaches EOF before finishing!

    struct CharToTokenType
    {
        char Char;
        TokenType type;
    };

    static const CharToTokenType map[] =
    {
        { '{', TokenType::BraceBegin },
        { '}', TokenType::BraceEnd },
        { '[', TokenType::BracketBegin },
        { ']', TokenType::BracketEnd },
        { ';', TokenType::Semicolon },
        { ',', TokenType::Comma },
        { '=', TokenType::Equals },
        { '<', TokenType::LessThan },
        { '>', TokenType::GreaterThan },
    };

    for (const CharToTokenType& m : map)
    {
        if (*cursor == m.Char)
        {
            token.token = std::string_view(cursor, cursor + 1);
            token.type = m.type;
            cursor++;
            return;
        }
    }

    token.token = std::string_view(cursor, cursor + 1);
    token.type = TokenType::Unknown;
    cursor++;

    return;
}

bool DefParser::ParseNamespacedIdentifier(const char*& cursor, Token& token)
{
    // This comes in with a token, which needs to be an identifier
    if (!TokenTypeExpected(token, TokenType::Identifier))
        return false;

    // In a loop:
    // If there not a "::" after the token, we are done.
    // If there is one, there needs to be an identifier after it too, and the string view expands to both
    while (true)
    {
        // eat the :: if there, else we are done
        if (!(cursor[0] == ':' && cursor[1] == ':'))
            return true;
        cursor += 2;

        // move to the end of the typical identifier
        while (IsValidSymbolCharacter(*cursor, false))
            cursor++;

        // extend the view
        token.token = std::string_view(token.token.data(), cursor - token.token.data());
    }

    return true;
}

bool DefParser::ParseStructDef(const char*& cursor)
{
    Token token;
    GetToken(cursor, token);
    if (!TokenTypeExpected(token, TokenType::Identifier))
        return false;

    Struct& newStruct = m_structs.emplace_back();
    newStruct.name = std::string(token.token);
    newStruct.nameSpace = m_currentNamespace;

    GetToken(cursor, token);
    if (!TokenTypeExpected(token, TokenType::BraceBegin))
        return false;

    // read the struct fields until we hit the end of the brace
    while (true)
    {
        GetToken(cursor, token);
        if (token.type == TokenType::BraceEnd)
            break;

        if (!ParseNamespacedIdentifier(cursor, token))
            return false;

        StructField& newField = newStruct.fields.emplace_back();

        // First try pod
        const Enum* e = nullptr;
        const Struct* s = nullptr;
        if (!IdentifierToFieldType(std::string(token.token).c_str(), newField.fieldType))
        {
            while (1)
            {
                // Try enum
                e = GetEnumByName(std::string(token.token).c_str());
                if (e)
                {
                    newField.fieldType = FieldType::_enum;
                    newField.enumName = e->FullName();
                    break;
                }

                // Try struct
                s = GetStructByName(std::string(token.token).c_str());
                if (s)
                {
                    newField.fieldType = FieldType::_struct;
                    newField.structName = s->FullName();
                    break;
                }

                // Try a link to a struct
                if (token.token == "Link")
                {
                    GetToken(cursor, token);
                    if (!TokenTypeExpected(token, TokenType::LessThan))
                        return false;

                    GetToken(cursor, token);
                    if (!ParseNamespacedIdentifier(cursor, token))
                        return false;

                    newField.fieldType = FieldType::_link;
                    newField.linkName = std::string(token.token);

                    if (!GetStructByName(newField.linkName.c_str()))
                    {
                        m_errorText << "Error in " << m_path << "(" << m_lineNumber << ") : struct name expected, got " << token.token;
                        return false;
                    }

                    GetToken(cursor, token);
                    if (!TokenTypeExpected(token, TokenType::GreaterThan))
                        return false;

                    break;
                }

                // Couldn't figure it out
                m_errorText << "Error in " << m_path << "(" << m_lineNumber << ") : type expected, got " << token.token;
                return false;
            }
        }

        GetToken(cursor, token);
        if (!TokenTypeExpected(token, TokenType::Identifier))
            return false;

        newField.name = std::string(token.token);

        GetToken(cursor, token);
        if (token.type == TokenType::Semicolon)
            continue;

        // Array
        if (token.type == TokenType::BracketBegin)
        {
            newField.isArray = true;

            GetToken(cursor, token);

            newField.fixedArraySize = 0;
            if (token.type == TokenType::LiteralInt)
            {
                newField.fixedArraySize = std::atoi(std::string(token.token).c_str());
                GetToken(cursor, token);
            }

            if (!TokenTypeExpected(token, TokenType::BracketEnd))
                return false;

            GetToken(cursor, token);
            if (!TokenTypeExpected(token, TokenType::Semicolon))
                return false;

            continue;
        }

        // Default
        if (!TokenTypeExpected(token, TokenType::Equals))
            return false;

        GetToken(cursor, token);

        switch (newField.fieldType)
        {
            case FieldType::_enum:
            {
                if (!TokenTypeExpected(token, TokenType::Identifier))
                    return false;

                if (!e->ContainsLabel(std::string(token.token).c_str()))
                {
                    m_errorText << "Error in " << m_path << "(" << m_lineNumber << ") : Unknown enum value: " << token.token;
                    return false;
                }
                newField.dflt = std::string(token.token);
                break;
            }
            case FieldType::_bool:
            {
                if (!TokenTypeExpected(token, TokenType::LiteralBool))
                    return false;
                newField.dflt = std::string(token.token);
                break;
            }
            case FieldType::_uint8:
            case FieldType::_sint8:
            case FieldType::_uint16:
            case FieldType::_sint16:
            case FieldType::_uint32:
            case FieldType::_sint32:
            case FieldType::_uint64:
            case FieldType::_sint64:
            {
                if (!TokenTypeExpected(token, TokenType::LiteralInt))
                    return false;
                newField.dflt = std::string(token.token);
                break;
            }
            case FieldType::_float:
            case FieldType::_double:
            {
                if (token.type != TokenType::LiteralInt)
                {
                    if (!TokenTypeExpected(token, TokenType::LiteralFloat))
                        return false;
                }
                newField.dflt = std::string(token.token);
                break;
            }
            default:
            {
                m_errorText << "Error in " << m_path << "(" << m_lineNumber << ") : unexpected: " << token.token;
                return false;
            }
        }

        GetToken(cursor, token);
        if (!TokenTypeExpected(token, TokenType::Semicolon))
            return false;
    }

    return true;
}

bool DefParser::ParseEnumDef(const char*& cursor)
{
    Token token;
    GetToken(cursor, token);
    if (!TokenTypeExpected(token, TokenType::Identifier))
        return false;

    Enum& newEnum = m_enums.emplace_back();
    newEnum.name = std::string(token.token);
    newEnum.nameSpace = m_currentNamespace;

    GetToken(cursor, token);
    if (!TokenTypeExpected(token, TokenType::BraceBegin))
        return false;

    // read the enum labels until we hit the end of the brace
    while (true)
    {
        GetToken(cursor, token);
        if (token.type == TokenType::BraceEnd)
            break;

        if (!TokenTypeExpected(token, TokenType::Identifier))
            return false;

        if (newEnum.ContainsLabel(std::string(token.token).c_str()))
        {
            m_errorText << "Error in " << m_path << ": duplicate enum value on line " << m_lineNumber;
            return false;
        }

        newEnum.labels.push_back(std::string(token.token));

        GetToken(cursor, token);
        if (token.type == TokenType::BraceEnd)
            break;

        if (!TokenTypeExpected(token, TokenType::Comma))
            return false;
    }

    return true;
}

bool DefParser::ParseDirectiveRoot(const char*& cursor)
{
    Token token;
    GetToken(cursor, token);
    if (!TokenTypeExpected(token, TokenType::Identifier))
        return false;

    m_rootStruct = std::string(token.token);

    if(!GetStructByName(m_rootStruct.c_str()))
    {
        m_errorText << "Error in " << m_path << "(" << m_lineNumber << ") : struct name expected, got " << token.token;
        return false;
    }

    return true;
}

bool DefParser::ParseDirectiveInclude(const char*& cursor)
{
    Token token;
    GetToken(cursor, token);
    if (!TokenTypeExpected(token, TokenType::LiteralString))
        return false;

    std::filesystem::path includePath = std::filesystem::path(m_path).remove_filename() / std::filesystem::path(token.token);
    includePath = std::filesystem::weakly_canonical(includePath);

    DefParser includeParser;
    if (!includeParser.Parse(includePath.generic_string().c_str()))
    {
        m_errorText << "Error in " << m_path << " when including " << token.token << "\n" << includeParser.GetErrorText();
        return false;
    }

    // Copy the types from the include
    m_structs.insert(m_structs.end(), includeParser.m_structs.begin(), includeParser.m_structs.end());
    m_enums.insert(m_enums.end(), includeParser.m_enums.begin(), includeParser.m_enums.end());

    return true;
}

bool DefParser::ParseNamespace(const char*& cursor)
{
    Token token;
    GetToken(cursor, token);
    if (!ParseNamespacedIdentifier(cursor, token))
        return false;

    m_currentNamespace = std::string(token.token);
    return true;
}

bool DefParser::TokenTypeExpected(const Token& token, TokenType expectedType)
{
    if (token.type == expectedType)
        return true;

    // TODO: write name of token in error too

    m_errorText << "Error loading " << m_path << "\n" << "Unexpected on line " << m_lineNumber << ": " << token.token;
    return false;
}

static void GetTypeNameAndNamespaceSearchPaths(const char* nameIn, const char* currentNamespace, std::string& nameOut, std::vector<std::string>& namespaces)
{
    // If no namespace in the name, check current namespace, then global namespace
    size_t namespaceEnd = std::string(nameIn).rfind("::");
    if (namespaceEnd == std::string::npos)
    {
        nameOut = nameIn;
        namespaces.push_back(currentNamespace);
        namespaces.push_back("");
        return;
    }

    // If there's a namspace in the name, only look in that namespace
    nameOut = &nameIn[namespaceEnd+2];
    namespaces.push_back(std::string(nameIn).substr(0, namespaceEnd));
}

const DefParser::Struct* DefParser::GetStructByName(const char* name) const
{
    std::string typeName;
    std::vector<std::string> namespaces;
    GetTypeNameAndNamespaceSearchPaths(name, m_currentNamespace.c_str(), typeName, namespaces);

    for (const std::string& n : namespaces)
    {
        for (const Struct& s : m_structs)
        {
            if (s.nameSpace == n && s.name == typeName)
                return &s;
        }
    }
    return nullptr;
}

const DefParser::Enum* DefParser::GetEnumByName(const char* name) const
{
    std::string typeName;
    std::vector<std::string> namespaces;
    GetTypeNameAndNamespaceSearchPaths(name, m_currentNamespace.c_str(), typeName, namespaces);

    for (const std::string& n : namespaces)
    {
        for (const Enum& e : m_enums)
        {
            if (e.nameSpace == n && e.name == typeName)
                return &e;
        }
    }
    return nullptr;
}

bool DefParser::Parse(const char* fileName)
{
    m_lineNumber = 1;
    m_path = fileName;

    std::string def;
    if (!LoadTextFile(fileName, def))
    {
        m_errorText << "Could not open file: " << fileName;
        return false;
    }

    const char* cursor = def.data();

    Token token;
    while (GetToken(cursor, token), token.type != TokenType::EndOfFile)
    {
        switch (token.type)
        {
            case TokenType::Namespace:
            {
                if (!ParseNamespace(cursor))
                    return false;
                break;
            }
            case TokenType::StructDef:
            {
                if (!ParseStructDef(cursor))
                    return false;
                break;
            }
            case TokenType::EnumDef:
            {
                if (!ParseEnumDef(cursor))
                    return false;
                break;
            }
            case TokenType::DirectiveRoot:
            {
                if (!ParseDirectiveRoot(cursor))
                    return false;
                break;
            }
            case TokenType::DirectiveInclude:
            {
                if (!ParseDirectiveInclude(cursor))
                    return false;
                break;
            }
            case TokenType::Semicolon:
            {
                int ijkl = 0;
                // no-op
                break;
            }
            default:
            {
                m_errorText << "Error loading " << m_path << "\n" << "Unexpected on line " << m_lineNumber << ": " << token.token;
                return false;
            }
        }
    }

    return true;
}

// TODO: standardize errors to have filename and line number before the text. maybe make a function
