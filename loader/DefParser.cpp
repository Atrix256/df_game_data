#include "DefParser.h"

#include <filesystem>

enum class TokenType : uint8_t
{
    Unknown,
    EndOfFile,
    Identifier,

    StructDef,
    EnumDef,

    TypeName,

    DirectiveRoot,
    DirectiveInclude,

    LiteralString,
    LiteralString_Unterminated,

    BraceBegin,
    BraceEnd,
    BracketBegin,
    BracketEnd,
    Semicolon,
    Comma,
    Equals,
};

static bool LoadTextFile(const char* fileName, std::string& contents)
{
    FILE* file = nullptr;
    fopen_s(&file, fileName, "rb");
    if (!file)
        return false;

    fseek(file, 0, SEEK_END);
    size_t fileSize = ftell(file);
    contents.resize(fileSize);
    fseek(file, 0, SEEK_SET);

    fread(contents.data(), 1, fileSize, file);

    fclose(file);

    return true;
}

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

    IdentifierToTokenType map[] =
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

    IdentifierToTokenType map[] =
    {
        {"struct", TokenType::StructDef},
        {"enum", TokenType::EnumDef},
        {"uint8", TokenType::TypeName},
        {"sint8", TokenType::TypeName},
        {"uint16", TokenType::TypeName},
        {"sint16", TokenType::TypeName},
        {"uint32", TokenType::TypeName},
        {"sint32", TokenType::TypeName},
        {"uint64", TokenType::TypeName},
        {"sint64", TokenType::TypeName},
        {"string", TokenType::TypeName},
        {"float", TokenType::TypeName},
        {"double", TokenType::TypeName},
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

void DefParser::GetToken(const char*& cursor, Token& token)
{
    SkipWhiteSpaceAndNewlines(cursor);

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

    const CharToTokenType map[] =
    {
        { '{', TokenType::BraceBegin },
        { '}', TokenType::BraceEnd },
        { '[', TokenType::BracketBegin },
        { ']', TokenType::BracketEnd },
        { ';', TokenType::Semicolon },
        { ',', TokenType::Comma },
        { '=', TokenType::Equals },
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

        StructField& newField = newStruct.fields.emplace_back();

        // TODO: i don't think type_name should be a token type. some are dynamic type names. should haev a function to see if an identifir is a type name.
        if (!TokenTypeExpected(token, TokenType::TypeName))
            return false;

        // TODO: convert the type name to a field type
        //newField.fieldType = 

        GetToken(cursor, token);
        if (!TokenTypeExpected(token, TokenType::Identifier))
            return false;

        newField.name = std::string(token.token);

        GetToken(cursor, token);
        if (token.type == TokenType::Semicolon)
            continue;

        if (!TokenTypeExpected(token, TokenType::Equals))
            return false;

        // TODO: need to read a value. token type can be literal_string, literal_integer, literal_float
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

        for (const std::string& s : newEnum.labels)
        {
            if (s == token.token)
            {
                m_errorText << "Error in " << m_path << ": duplicate enum value on line " << m_lineNumber;
                return false;
            }
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

    // TODO: ensure it's a known struct type! error if not. GetStructByName should deal with namespaces. maybe get rid of namespaces?
    // TODO: or maybe it looks in the current namespace, before the global one. but the type may have the namespace as part of the type.

    m_rootStruct = std::string(token.token);
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

bool DefParser::TokenTypeExpected(const Token& token, TokenType expectedType)
{
    if (token.type == expectedType)
        return true;

    // TODO: write name of token in error too

    m_errorText << "Error loading " << m_path << "\n" << "Unexpected on line " << m_lineNumber << ": " << token.token;
    return false;
}

// TODO: GetStructByName and GetEnumByName need to deal with namespaces:
// 1) If name has a namespace, search that namespace.
// 2) Else
//   a) Search the current namespace
//   b) Then search the global namespace

const DefParser::Struct* DefParser::GetStructByName(const char* name) const
{
    for (const Struct& s : m_structs)
    {
        if (name == s.name)
            return &s;
    }
    return nullptr;
}

const DefParser::Enum* DefParser::GetEnumByName(const char* name) const
{
    for (const Enum& e : m_enums)
    {
        if (name == e.name)
            return &e;
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
