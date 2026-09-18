#include "loader.h"

#include <fstream>
#include <filesystem>

bool DBTable::LoadSchema()
{
    if (!m_defParser.Parse(m_path.c_str()))
    {
        // TODO: clean this up when it's working
        m_errorText = m_defParser.GetErrorText();
        return false;
    }

    m_parser.opts.strict_json = true;
    m_parser.opts.output_default_scalars_in_json = true;
    m_parser.opts.output_enum_identifiers = true;

    if (!flatbuffers::LoadFile(m_path.c_str(), false, &m_fbsFile))
    {
        m_errorText = "Could not load schema: " + m_path;
        return false;
    }

    // insert after the last include:
    // attribute "link";
    {
        size_t lastIncludePos = m_fbsFile.rfind("include \"");
        if (lastIncludePos != std::string::npos)
            lastIncludePos = m_fbsFile.find("\"", lastIncludePos + 9);
        if (lastIncludePos != std::string::npos)
            lastIncludePos = m_fbsFile.find("\n", lastIncludePos + 1);
        if (lastIncludePos == std::string::npos)
            lastIncludePos = 0;
        else
            lastIncludePos++;
        m_fbsFile.insert(lastIncludePos, "attribute \"link\";\n");
    }

    m_includeDirsStr.push_back(std::filesystem::path(m_path).remove_filename().generic_string());

    m_includeDirs.resize(m_includeDirsStr.size());
    for (size_t i = 0; i < m_includeDirs.size(); ++i)
        m_includeDirs[i] = m_includeDirsStr[i].c_str();

    if (!m_parser.Parse(m_fbsFile.c_str(), m_includeDirs.data(), m_path.c_str()))
    {
        m_errorText = "Error when loading schema: " + m_path + "\n" + m_parser.error_;
        return false;
    }
    else if(!m_parser.error_.empty())
    {
        m_errorText = "Warning when loading schema: " + m_path + "\n" + m_parser.error_;
    }

    if (!m_parser.root_struct_def_)
    {
        m_errorText = "No root type specified for schema: " + m_path;
        return false;
    }
    m_rootType = m_parser.root_struct_def_->name;

    return true;
}

bool DBTable::LoadData()
{
    // Get a file iterator for this directory
    std::error_code ec;
    std::filesystem::recursive_directory_iterator it(std::filesystem::path(m_path).remove_filename(), ec);
    if (ec)
    {
        m_errorText = "Could not scan directory: " + std::filesystem::path(m_path).remove_filename().generic_string();
        return false;
    }

    // Loop through all the files
    while (it != std::filesystem::recursive_directory_iterator{})
    {
        // Use the noexcept overload of is_regular_file
        const auto& entry = *it;
        if (std::filesystem::is_regular_file(entry, ec) && entry.path().extension() == ".json")
        {
            if (!LoadFile(entry.path().generic_string().c_str()))
                return false;
        }

        it.increment(ec);
        if (ec)
        {
            m_errorText = "Error while scanning directory: " + std::filesystem::path(m_path).remove_filename().generic_string() + "\n" + ec.message();
            return false;
        }
    }

    return true;
}

bool DBTable::LoadFile(const char* fileName)
{
    // Load the json file
    std::string jsonString;
    if (!flatbuffers::LoadFile(fileName, false, &jsonString))
    {
        m_errorText = "Could not load data file: " + std::string(fileName);
        return false;
    }

    // Make sure it conforms to the schema
    if (!m_parser.Parse(jsonString.c_str(), nullptr, fileName))
    {
        m_errorText = "Could not load data file: " + std::string(fileName) + "\n" + m_parser.error_;
        return false;
    }

    // Load the data using nlohmann since it's easier to work with
    json data = json::parse(jsonString, nullptr, false);
    if (data.is_discarded())
    {
        // No error text available from nlohmann.
        // This is a weird error because flatbuffers loaded it just fine.
        m_errorText = "Could not load parse data file: " + std::string(fileName) + "\n" + m_parser.error_;
        return false;
    }

    // Insert the data into the data table.
    // The filename without extension is the key.
    std::unique_ptr<JSONData> newData = std::make_unique<JSONData>();
    std::string key = std::filesystem::path(fileName).filename().replace_extension("").generic_string();
    newData->m_data = data;
    newData->m_path = fileName;
    m_data[key] = std::move(newData);

    return true;
}

bool DBTable::Load(const char* path)
{
    m_path = path;

    if (!LoadSchema())
        return false;

    if (!LoadData())
        return false;

    return true;
}

void DBRoot::LoadSettings(json& data)
{
    if (data.contains("compileOutputDir"))
        m_settings.compileOutputDir = data.at("compileOutputDir");

    if (data.contains("nameSpace"))
        m_settings.nameSpace = data.at("nameSpace");

    if (data.contains("targetLanguage"))
        m_settings.targetLanguage = data.at("targetLanguage");
}

void DBRoot::SaveDBRoot()
{
    // Process the tables in the order specified in the dbroot, because that matters for declarations
    std::vector<std::string> tableOrder(m_tables.size());
    for (const auto& pair : m_tables)
        tableOrder[pair.second->m_loadOrder] = pair.first;

    json doc = json::object();
    doc["compileOutputDir"] = m_settings.compileOutputDir;
    doc["nameSpace"] = m_settings.nameSpace;
    doc["targetLanguage"] = m_settings.targetLanguage;

    doc["tables"] = json::array();
    std::filesystem::path basePath = std::filesystem::path(m_path).remove_filename();
    for (const std::string& tableName : tableOrder)
    {
        std::filesystem::path target(m_tables[tableName]->GetPath());
        doc["tables"].push_back(std::filesystem::proximate(target, basePath).generic_string());
    }

    std::string jsonString = doc.dump(4);

    FILE* file = nullptr;
    fopen_s(&file, m_path.c_str(), "wb");
    if (file)
    {
        fwrite(jsonString.c_str(), 1, jsonString.size(), file);
        fclose(file);
    }
}

bool DBRoot::RemoveTable(const char* name)
{
    if (!m_tables.contains(name))
        return false;

    // Remove the file watch
    m_fileWatcher.RemoveDirectory(std::filesystem::path(m_tables[name]->GetPath()).remove_filename().generic_string().c_str());

    // remove the table
    m_tables.erase(name);

    // renumber the load orders of the tables
    struct LoadOrder
    {
        int loadOrder;
        std::string name;
    };

    std::vector<LoadOrder> loadOrder;
    for (const auto& pair : m_tables)
        loadOrder.push_back({ pair.second->m_loadOrder, pair.first });

    std::sort(loadOrder.begin(), loadOrder.end(),
        [this] (const LoadOrder& A, const LoadOrder& B)
        {
            return m_tables[A.name]->m_loadOrder < m_tables[B.name]->m_loadOrder;
        }
    );

    int index = 0;
    for (const LoadOrder& order : loadOrder)
        m_tables[order.name]->m_loadOrder = index++;

    return true;
}

bool DBRoot::AddTable(const char* path)
{
    std::unique_ptr<DBTable> newTable = std::make_unique<DBTable>();
    if (!newTable->Load(path))
    {
        if (!m_errorText.empty())
            m_errorText += std::string("\n\n");
        m_errorText += newTable->GetErrorText();
        return false;
    }

    m_fileWatcher.AddDirectory(std::filesystem::path(path).remove_filename().generic_string().c_str(), nullptr);

    // accumulate warnings
    std::string warningText = newTable->GetErrorText();
    if (!warningText.empty())
    {
        if (!m_errorText.empty())
            m_errorText += std::string("\n\n");
        m_errorText += warningText;
    }

    newTable->m_loadOrder = (int)m_tables.size();

    if (m_tables.contains(newTable->m_rootType))
    {
        m_errorText = "Table already exists in database: " + newTable->m_rootType;
        return false;
    }

    m_tables[newTable->m_rootType] = std::move(newTable);

    return true;
}

bool DBRoot::New(const char* path)
{
    Clear();

    FILE* file = nullptr;
    fopen_s(&file, path, "wb");
    if (!file)
    {
        m_errorText = "Could not open for writing: " + std::string(path);
        return false;
    }

    fprintf(file, "{\n    \"tables\": []\n}\n");
    fclose(file);

    return Load(path);
}

bool DBRoot::Load(const char* path)
{
    Clear();

    std::string extension = std::filesystem::path(path).extension().generic_string();
    std::filesystem::path base_path = std::filesystem::absolute(path).remove_filename();

    if (extension == ".dbroot")
    {
        m_path = path;

        std::string jsonString;
        if (!flatbuffers::LoadFile(path, false, &jsonString))
        {
            m_errorText = "Failed to open dbroot file: " + std::string(path);
            return false;
        }

        json data = json::parse(jsonString, nullptr, false);
        if (data.is_discarded())
        {
            m_errorText = "Failed to parse dbroot file: " + std::string(path);
            return false;
        }

        m_fileWatcher.AddFile(path, nullptr);

        // Load the tables
        int index = 0;
        while(true)
        {
            json_pointer jsonPath("/tables");
            jsonPath /= index++;

            if (!data.contains(jsonPath))
                break;

            std::string tablePath = data.value<std::string>(jsonPath, "");

            std::filesystem::path full_path = std::filesystem::weakly_canonical(base_path / tablePath);

            if (!AddTable(full_path.generic_string().c_str()))
            {
                Clear();
                return false;
            }
        }
    }
    else if (extension == ".fbs")
    {
        // If given a .fbs file, make a .dbsroot file containing only that item, and load that
        std::filesystem::path dbroot = std::filesystem::path(path).replace_extension(".dbroot");
        FILE* file = nullptr;
        fopen_s(&file, dbroot.generic_string().c_str(), "wb");
        if (!file)
        {
            m_errorText = "Could not open for writing: " + dbroot.generic_string();
            return false;
        }

        fprintf(file, "{\n    \"tables\": [\n        \"%s\"\n    ]\n}\n", std::filesystem::path(path).filename().generic_string().c_str());
        fclose(file);

        return Load(dbroot.generic_string().c_str());
    }
    else
    {
        m_errorText = "Unknown file type: " + std::string(path);
        return false;
    }

    return true;
}

void DBRoot::Clear()
{
    m_tables.clear();
    m_path = "";
    m_fileWatcher.Clear();
}
