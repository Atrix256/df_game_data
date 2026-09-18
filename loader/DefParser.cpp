#include "DefParser.h"

struct ParserState
{
    int lineNumber = 1;
};

static ParserState s_parserState;

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

static bool SkipWhiteSpaceAndNewlines(const char*& cursor)
{
    bool ret = false;
    while (std::isspace(*cursor))
    {
        if (*cursor == '\n')
            s_parserState.lineNumber++;
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

static bool SkipWhiteSpaceAndNewlinesAndComments(const char*& cursor)
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
        {"#root", TokenType::directive_root},
    };

    for (const IdentifierToTokenType& m : map)
    {
        if (token.token == m.identifier)
        {
            token.type = m.type;
            return;
        }
    }
    token.type = TokenType::unknown;
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
        {"struct", TokenType::struct_def},
        {"uint8", TokenType::type_name},
        {"sint8", TokenType::type_name},
        {"uint16", TokenType::type_name},
        {"sint16", TokenType::type_name},
        {"uint32", TokenType::type_name},
        {"sint32", TokenType::type_name},
        {"uint64", TokenType::type_name},
        {"sint64", TokenType::type_name},
        {"string", TokenType::type_name},
        {"float", TokenType::type_name},
        {"double", TokenType::type_name},
    };

    for (const IdentifierToTokenType& m : map)
    {
        if (token.token == m.identifier)
        {
            token.type = m.type;
            return;
        }
    }

    token.type = TokenType::identifier;
}

static void GetToken(const char*& cursor, Token& token)
{
    SkipWhiteSpaceAndNewlines(cursor);

    if (*cursor == 0)
    {
        token.token = std::string_view(cursor, cursor+1);
        token.type = TokenType::end_of_file;
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
        token.type = TokenType::identifier;
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
        token.type = TokenType::identifier;
        ConvertDirectiveToken(token);
        return;
    }
    else
    {
        // TODO: look for...
        // string literals (quoted)
        // integer literals
        // float literals
        // for enums, they will want an identifer
    }

    struct CharToTokenType
    {
        char Char;
        TokenType type;
    };

    const CharToTokenType map[] =
    {
        { '{', TokenType::brace_begin },
        { '}', TokenType::brace_end },
        { '[', TokenType::bracket_begin },
        { ']', TokenType::bracket_end },
        { ';', TokenType::semicolon },
        { '=', TokenType::equals },
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
    token.type = TokenType::unknown;
    cursor++;

    return;
}

bool DefParser::ParseStructDef(const char*& cursor)
{
    Token token;
    GetToken(cursor, token);
    if (!TokenTypeExpected(token, TokenType::identifier))
        return false;

    Type& newStruct = m_types.emplace_back();
    newStruct.name = std::string(token.token);
    newStruct.nameSpace = m_currentNamespace;

    GetToken(cursor, token);
    if (!TokenTypeExpected(token, TokenType::brace_begin))
        return false;

    // read the struct fields until we hit the end of the brace
    while (true)
    {
        GetToken(cursor, token);
        if (token.type == TokenType::brace_end)
            break;

        TypeField& newField = newStruct.fields.emplace_back();

        // TODO: i don't think type_name should be a token type. some are dynamic type names. should haev a function to see if an identifir is a type name.
        if (!TokenTypeExpected(token, TokenType::type_name))
            return false;

        // TODO: convert the type name to a field type
        //newField.fieldType = 

        GetToken(cursor, token);
        if (!TokenTypeExpected(token, TokenType::identifier))
            return false;

        newField.name = std::string(token.token);

        GetToken(cursor, token);
        if (token.type == TokenType::semicolon)
            continue;

        if (!TokenTypeExpected(token, TokenType::equals))
            return false;

        // TODO: need to read a value. token type can be literal_string, literal_integer, literal_float
    }

    return true;
}

bool DefParser::ParseDirectiveRoot(const char*& cursor)
{
    Token token;
    GetToken(cursor, token);
    if (!TokenTypeExpected(token, TokenType::identifier))
        return false;

    m_rootType = std::string(token.token);
    return true;
}

bool DefParser::TokenTypeExpected(const Token& token, TokenType expectedType)
{
    if (token.type == expectedType)
        return true;

    m_errorText << "Error loading " << m_path << "\n" << "Unexpected on line " << s_parserState.lineNumber << ": " << token.token;
    return false;
}

bool DefParser::Parse(const char* fileName)
{
    s_parserState = ParserState();
    m_path = fileName;

    std::string def;
    if (!LoadTextFile(fileName, def))
        return false;

    const char* cursor = def.data();

    Token token;
    while (GetToken(cursor, token), token.type != TokenType::end_of_file)
    {
        switch (token.type)
        {
            case TokenType::struct_def:
            {
                if (!ParseStructDef(cursor))
                    return false;
                break;
            }
            case TokenType::directive_root:
            {
                if (!ParseDirectiveRoot(cursor))
                    return false;
                break;
            }
            case TokenType::semicolon:
            {
                // no-op
                break;
            }
            default:
            {
                m_errorText << "Error loading " << m_path << "\n" << "Unexpected on line " << s_parserState.lineNumber << ": " << token.token;
                return false;
            }
        }
    }

    return true;
}
