#include "loader.h"

#include <fstream>
#include <filesystem>

bool DBTable::LoadSchema()
{
    m_parser.opts.strict_json = true;
    m_parser.opts.output_default_scalars_in_json = true;
    m_parser.opts.output_enum_identifiers = true;

    if (!flatbuffers::LoadFile(m_path.c_str(), false, &m_fbsFile))
    {
        m_errorText = "Could not load schema: " + m_path;
        return false;
    }

    // insert after the last include:
    // attribute "root_type";
    // attribute "link";
    {
        size_t lastIncludePos = m_fbsFile.rfind("include \"");
        if (lastIncludePos != std::string::npos)
            lastIncludePos = m_fbsFile.find("\"", lastIncludePos + 9);
        if (lastIncludePos != std::string::npos)
            lastIncludePos = m_fbsFile.find("\n", lastIncludePos + 1);
        if (lastIncludePos == std::string::npos)
            lastIncludePos = 0;
        m_fbsFile.insert(lastIncludePos, "\nattribute \"root_type\";\nattribute \"link\";");
    }

    m_includeDirsStr.push_back(std::filesystem::path(m_path).remove_filename().string());

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

    // Get the one (no less, no more) root_type
    bool rootTypeFound = false;
    for (auto& structDef : m_parser.structs_.vec)
    {
        if (structDef->attributes.Lookup("root_type") != nullptr)
        {
            if (rootTypeFound)
            {
                m_errorText = "Multiple root types found in schema: " + m_path;
                return false;
            }
            m_rootType = structDef->name;
            rootTypeFound = true;
        }
    }
    if (!rootTypeFound)
    {
        m_errorText = "No root type found, please add the (root_type) attribute to one struct.\n" + m_path;
        return false;
    }

    // set the root type on the parser
    m_parser.SetRootType(m_rootType.c_str());

    return true;
}

bool DBTable::LoadData()
{
    // Get a file iterator for this directory
    std::error_code ec;
    std::filesystem::recursive_directory_iterator it(std::filesystem::path(m_path).remove_filename(), ec);
    if (ec)
    {
        m_errorText = "Could not scan directory: " + std::filesystem::path(m_path).remove_filename().string();
        return false;
    }

    // Loop through all the files
    while (it != std::filesystem::recursive_directory_iterator{})
    {
        // Use the noexcept overload of is_regular_file
        const auto& entry = *it;
        if (std::filesystem::is_regular_file(entry, ec) && entry.path().extension() == ".json")
        {
            if (!LoadFile(entry.path().string().c_str()))
                return false;
        }

        it.increment(ec);
        if (ec)
        {
            m_errorText = "Error while scanning directory: " + std::filesystem::path(m_path).remove_filename().string() + "\n" + ec.message();
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
    std::string key = std::filesystem::path(fileName).filename().replace_extension("").string();
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

bool DBRoot::Load(const char* path)
{
    Clear();

    // Load m_tables
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            m_errorText = "Failed to open dbroot file: " + std::string(path);
            return false;
        }

        m_fileWatcher.AddFile(path, nullptr);

        std::filesystem::path base_path = std::filesystem::absolute(path).remove_filename();

        std::string line;
        while (std::getline(file, line))
        {
            std::filesystem::path full_path = std::filesystem::weakly_canonical(base_path / line);

            std::unique_ptr<DBTable> newTable = std::make_unique<DBTable>();
            if (!newTable->Load(full_path.string().c_str()))
            {
                m_errorText = newTable->GetErrorText();
                Clear();
                file.close();
                return false;
            }

            m_fileWatcher.AddDirectory(full_path.remove_filename().string().c_str(), nullptr);

            // accumulate warnings
            std::string warningText = newTable->GetErrorText();
            if (!warningText.empty())
            {
                if (!m_errorText.empty())
                    m_errorText += std::string("\n\n");
                m_errorText += warningText;
            }

            m_tables[newTable->m_rootType] = std::move(newTable);
        }

        file.close();
    }

    return true;
}

void DBRoot::Clear()
{
    m_tables.clear();
}
