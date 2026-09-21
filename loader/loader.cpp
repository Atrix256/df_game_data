#include "loader.h"

#include <fstream>
#include <filesystem>
#include <algorithm>

bool DBTable::LoadSchema()
{
    if (!m_parser.Parse(m_path.c_str()))
    {
        m_errorText = m_parser.GetErrorText();
        return false;
    }
    m_rootType = m_parser.GetRootStructName();

    if (m_rootType.empty())
    {
        m_errorText = "no #root specified in " + m_path;
        return false;
    }

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
    if (!LoadTextFile(fileName, jsonString))
    {
        m_errorText = "Could not load data file: " + std::string(fileName);
        return false;
    }

    // Load the data using nlohmann since it's easier to work with
    json data = json::parse(jsonString, nullptr, false);
    if (data.is_discarded())
    {
        // No error text available from nlohmann.
        // This is a weird error because flatbuffers loaded it just fine.
        m_errorText = "Could not load parse data file: " + std::string(fileName);
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
    json_pointer basePtr("/compilersettings");

    uint32_t settingsCount = 0;
    if (data.contains(basePtr))
        settingsCount = (uint32_t)data.value(basePtr, json::array()).size();

    // Make a row for each setting row in the json.
    // If there are no setting rows in the json, make one default row.
    m_compileSettings.clear();
    m_compileSettings.resize(std::max<uint32_t>(settingsCount, 1));

    for (uint32_t index = 0; index < settingsCount; ++index)
    {
        json_pointer indexPtr = basePtr / index;

        if (data.contains(indexPtr / "compiledHeaderFileName"))
            m_compileSettings[index].compiledHeaderFileName = data.at(indexPtr / "compiledHeaderFileName");

        if (data.contains(indexPtr / "compiledBinFileName"))
            m_compileSettings[index].compiledBinFileName = data.at(indexPtr / "compiledBinFileName");

        if (data.contains(indexPtr / "className"))
            m_compileSettings[index].className = data.at(indexPtr / "className");

        if (data.contains(indexPtr / "includeEntryLUT"))
            m_compileSettings[index].includeEntryLUT = data.at(indexPtr / "includeEntryLUT");
    }
}

void DBRoot::SaveDBRoot()
{
    // Process the tables in the order specified in the dbroot, because that matters for declarations
    std::vector<std::string> tableOrder(m_tables.size());
    for (const auto& pair : m_tables)
        tableOrder[pair.second->m_loadOrder] = pair.first;

    json doc = json::object();

    // write compiler settings
    {
        auto compilerSettingsArray = json::array();

        for (uint32_t index = 0; index < (uint32_t)m_compileSettings.size(); ++index)
        {
            const DBCompileSettings& setting = m_compileSettings[index];

            auto compilerSettingObject = json::object();

            compilerSettingObject["compiledHeaderFileName"] = setting.compiledHeaderFileName;
            compilerSettingObject["compiledBinFileName"] = setting.compiledBinFileName;
            compilerSettingObject["className"] = setting.className;
            compilerSettingObject["includeEntryLUT"] = setting.includeEntryLUT;

            compilerSettingsArray.push_back(compilerSettingObject);
        }

        doc["compilersettings"] = compilerSettingsArray;
    }

    // Write the tables
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
        if (!LoadTextFile(path, jsonString))
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

        // Load the settings
        LoadSettings(data);

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
    else if (extension == ".def")
    {
        // If given a .def file, make a .dbsroot file containing only that item, and load that
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

bool LoadTextFile(const char* fileName, std::string& contents)
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
};
