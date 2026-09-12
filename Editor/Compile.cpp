#include "Compile.h"

#include "flatbuffers/idl.h"
#include "../loader/JSON.h"
#include "Editor.h"

extern bool RunFlatc(const char* args, bool waitForExit);
extern std::string GetProcessTempDirectory();

std::filesystem::path ApplyPath(const std::filesystem::path& base, const std::filesystem::path& input)
{
    if (input.is_absolute())
        return input;
    return base / input;
}

std::string ExtractLinesWithPrefix(std::string& text, const std::string& prefix)
{
    std::string extracted;
    std::string result;
    result.reserve(text.size());

    size_t pos = 0;
    while (pos <= text.size())
    {
        size_t lineEnd = text.find('\n', pos);
        bool hasNewline = (lineEnd != std::string::npos);
        size_t actualEnd = hasNewline ? lineEnd : text.size();

        // The line, including its trailing '\n' if present
        std::string line = text.substr(pos, actualEnd - pos + (hasNewline ? 1 : 0));

        // Find first non-whitespace char (excluding the line's own content) to check prefix
        size_t firstNonWs = 0;
        while (firstNonWs < actualEnd - pos && std::isspace(static_cast<unsigned char>(text[pos + firstNonWs])))
            ++firstNonWs;

        bool matches = false;
        size_t remaining = (actualEnd - pos) - firstNonWs;
        if (remaining >= prefix.size())
            matches = (text.compare(pos + firstNonWs, prefix.size(), prefix) == 0);

        if (matches)
            extracted += line;
        else
            result += line;

        if (!hasNewline)
            break;

        pos = lineEnd + 1;
    }

    text = std::move(result);
    return extracted;
}

bool CompileData(EditorData& editorData)
{
    if (editorData.m_dbroot.m_tables.size() == 0)
        return false;

    std::string dbRootPath = std::filesystem::path(editorData.m_dbroot.GetPath()).remove_filename().generic_string();
    std::string outputDir = ApplyPath(dbRootPath, editorData.m_settings.compileOutputDir).generic_string();
    std::string tempDir = GetProcessTempDirectory();

    // Make and write out the combined schema file from all the input tables
    {
        std::string includes;
        std::string combinedSchema;
        std::string includePaths;

        // Process the tables in the order specified in the dbroot, because that matters for declarations
        std::vector<std::string> tableOrder(editorData.m_dbroot.m_tables.size());
        for (const auto& pair : editorData.m_dbroot.m_tables)
            tableOrder[pair.second->m_loadOrder] = pair.first;

        for (const std::string& tableName : tableOrder)
        {
            const DBTable& table = *editorData.m_dbroot.m_tables[tableName].get();

            // load the schema
            std::string schemaString;
            if (!flatbuffers::LoadFile(table.GetPath(), false, &schemaString))
                return false;

            // remove the root_type line since we are
            ExtractLinesWithPrefix(schemaString, "root_type");

            // take the includes out
            includes += ExtractLinesWithPrefix(schemaString, "include");

            // Add what's left to the combined schema
            combinedSchema += schemaString;

            // we need to track all the include paths
            // If there's a problem with this (picks wrong paths for same file names), we will need to rewrite
            // the include lines to absolute paths instead
            includePaths += " -I " + std::filesystem::path(table.GetPath()).remove_filename().generic_string();
        }

        std::string fullSchema = includes + "attribute \"link\";\n" + combinedSchema;

        std::string fullSchemaFileName = std::filesystem::path(tempDir).replace_filename("schema.fbs").generic_string();

        FILE* file = nullptr;
        fopen_s(&file, fullSchemaFileName.c_str(), "wb");
        if (file)
        {
            fwrite(fullSchema.c_str(), 1, fullSchema.size(), file);
            fclose(file);
        }
        else
        {
            return false;
        }

        std::string commandLine = "--cpp" + includePaths + " -o \"" + outputDir + "\" \"" + fullSchemaFileName + "\"";
        if (!RunFlatc(commandLine.c_str(), false))
            return false;
    }

    if (editorData.m_dbroot.m_tables.count(editorData.m_selectedTableName) > 0)
    {
        DBTable& table = *editorData.m_dbroot.m_tables[editorData.m_selectedTableName].get();

        if (table.m_data.count(editorData.m_selectedDataItemName) > 0)
        {
            DBTable::JSONData& data = *table.m_data[editorData.m_selectedDataItemName].get();

            // Make a binary file
            {
                std::string commandLine = "-b -o \"" + outputDir + "\" \"" + std::string(table.GetPath()) + "\" \"" + data.m_path + "\"";
                if (!RunFlatc(commandLine.c_str(), false))
                    return false;
            }
        }
    }
    return true;
}
