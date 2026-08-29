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

    m_fbsFile = std::string("attribute \"root_type\";\n") + m_fbsFile;

    m_includeDirsStr.push_back(std::filesystem::path(m_path).remove_filename().string());

    m_includeDirs.resize(m_includeDirsStr.size());
    for (size_t i = 0; i < m_includeDirs.size(); ++i)
        m_includeDirs[i] = m_includeDirsStr[i].c_str();

    if (!m_parser.Parse(m_fbsFile.c_str(), m_includeDirs.data(), m_path.c_str()))
    {
        m_errorText = "Error when loading schema: " + m_path + "\n" + m_parser.error_;
        return false;
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

    return true;
}

bool DBTable::LoadData()
{
    // TODO: this! Find all the data files, load them, make sure they follow the schema
    // TODO: may not need to make the binary version of the schema.
    // #include "flatbuffers/idl.h"
    // #include "flatbuffers/util.h"
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

        std::filesystem::path base_path = std::filesystem::absolute(path).remove_filename();

        std::string line;
        while (std::getline(file, line))
        {
            std::filesystem::path full_path = std::filesystem::weakly_canonical(base_path / line);

            DBTable& newTable = m_tables.emplace_back();
            if (!newTable.Load(full_path.string().c_str()))
            {
                m_errorText = newTable.GetErrorText();
                Clear();
                file.close();
                return false;
            }
        }

        std::sort(m_tables.begin(), m_tables.end(), [](const DBTable& a, const DBTable& b) {
            return strcmp(a.GetName(), b.GetName()) < 0;
        });

        file.close();
    }

    return true;
}

void DBRoot::Clear()
{
    m_tables.clear();
}
