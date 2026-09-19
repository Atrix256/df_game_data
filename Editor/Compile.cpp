#include "Compile.h"

#include "../loader/JSON.h"
#include "Editor.h"
#include "Platform.h"

inline void StringReplaceAll(std::string& str, const std::string& from, const std::string& to)
{
    if (from.empty())
        return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos)
    {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

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
    std::string outputDir = ApplyPath(dbRootPath, editorData.m_dbroot.m_settings.compileOutputDir).generic_string();
    std::string tempDir = GetProcessTempDirectory();

    // Process the tables in the order specified in the dbroot, because that matters for declarations
    std::vector<std::string> tableOrder(editorData.m_dbroot.m_tables.size());
    for (const auto& pair : editorData.m_dbroot.m_tables)
        tableOrder[pair.second->m_loadOrder] = pair.first;

    // Make the combined schema from all tables and compile it
    std::string fullSchemaFileName;
    std::string includePaths;
    {
        std::string includes;
        std::string combinedSchema;

        for (const std::string& tableName : tableOrder)
        {
            const DBTable& table = *editorData.m_dbroot.m_tables[tableName].get();

            // load the schema
            std::string schemaString;
            if (!LoadTextFile(table.GetPath(), schemaString))
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

        std::string fullSchema = includes + "attribute \"link\";\n";

        if (!editorData.m_dbroot.m_settings.nameSpace.empty())
            fullSchema += "namespace " + editorData.m_dbroot.m_settings.nameSpace + ";\n";

        fullSchema += combinedSchema;

        // make the schema for the table that will map entry names to indices.
        fullSchema += "\ntable EntryNameToIndex\n{\n  name:string;\n  index:uint32;\n}\n\n";

        // Make a table that has arrays of each table type, and a mapping from sorted name to index
        // Make this table the root type
        fullSchema += "table dbroot\n{\n";
        for (const std::string& tableName : tableOrder)
        {
            std::string memberNameBase = tableName;
            std::transform(memberNameBase.begin(), memberNameBase.end(), memberNameBase.begin(),
                [](unsigned char c)
                {
                    return std::tolower(c);
                }
            );

            fullSchema += "    " + memberNameBase + "_entries:[" + tableName + "];\n";

            fullSchema += "    " + memberNameBase + "_name_to_index:[EntryNameToIndex];\n";
        }
        fullSchema += "}\n\nroot_type dbroot;\n\n";

        fullSchemaFileName = std::filesystem::path(tempDir).replace_filename("schema.def").generic_string();

        FILE* file = nullptr;
        fopen_s(&file, fullSchemaFileName.c_str(), "wb");
        if (!file)
            return false;
        fwrite(fullSchema.c_str(), 1, fullSchema.size(), file);
        fclose(file);

        std::string commandLine = "--" + editorData.m_dbroot.m_settings.targetLanguage + includePaths + " -o \"" + outputDir + "\" \"" + fullSchemaFileName + "\"";
        if (!RunFlatc(commandLine.c_str(), true))
            return false;
    }

    // Make the combined data from all tables and compile it
    {
        std::string allData = "{\n";

        for (const std::string& tableName : tableOrder)
        {
            const DBTable& table = *editorData.m_dbroot.m_tables[tableName].get();

            std::string memberName = tableName;
            std::transform(memberName.begin(), memberName.end(), memberName.begin(),
                [](unsigned char c)
                {
                    return std::tolower(c);
                }
            );

            std::string allDataItems = "    \"" + memberName + "_entries\" : [\n";

            for (const auto& pair : table.m_data)
            {
                // load the json data
                std::string jsonString;
                if (!LoadTextFile(pair.second->m_path.c_str(), jsonString))
                    return false;

                StringReplaceAll(jsonString, "\n", "\n        ");

                allDataItems += "        " + jsonString + ",\n";
            }

            // remove trailing comma
            allDataItems.pop_back();
            allDataItems.pop_back();
            allDataItems.push_back('\n');

            allDataItems += "    ],\n";

            // Add the name_to_index sorted list
            {
                struct NameToIndex
                {
                    std::string name;
                    int index;
                };
                std::vector<NameToIndex> nameToIndex;
                int i = 0;
                for (const auto& pair : table.m_data)
                    nameToIndex.push_back({ pair.first, i++ });

                std::sort(nameToIndex.begin(), nameToIndex.end(),
                    [](const NameToIndex& A, const NameToIndex& B)
                    {
                        return A.name < B.name;
                    }
                );

                allDataItems += "    \"" + memberName + "_name_to_index\" : [\n";
                for (const NameToIndex& n : nameToIndex)
                    allDataItems += "        { name:\"" + n.name + "\", index:" + std::to_string(n.index) + " },\n";

                // remove trailing comma
                allDataItems.pop_back();
                allDataItems.pop_back();
                allDataItems.push_back('\n');

                allDataItems += "    ],\n";
            }

            allData += allDataItems;
        }

        // remove trailing comma
        allData.pop_back();
        allData.pop_back();
        allData.push_back('\n');

        allData += "}\n";

        // write it out
        std::string fullDataFileName = std::filesystem::path(tempDir).replace_filename("data.json").generic_string();

        FILE* file = nullptr;
        fopen_s(&file, fullDataFileName.c_str(), "wb");
        if (!file)
            return false;
        fwrite(allData.c_str(), 1, allData.size(), file);
        fclose(file);

        std::string commandLine = "-b" + includePaths + " -o \"" + outputDir + "\" \"" + fullSchemaFileName + "\" \"" + fullDataFileName + "\"";
        if (!RunFlatc(commandLine.c_str(), false))
            return false;
    }

    return true;
}
