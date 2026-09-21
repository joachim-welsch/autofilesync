#include "SandboxTree.h"
#include <algorithm>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace guitest {

SandboxTree::SandboxTree(fs::path root, const FileContents& files)
    : root_(std::move(root))
{
    fs::remove_all(root_);  // [SandboxTree.md#fresh-start]
    fs::create_directories(root_);
    for (const auto& [relative, content] : files) {
        if (relative.filename().empty())
            fs::create_directories(root_ / relative);
        else
            write(relative, content);
    }
}

SandboxTree::~SandboxTree()
{
    std::error_code ignored;
    fs::remove_all(root_, ignored);
}

void SandboxTree::write(const fs::path& relative, const std::string& content) const
{
    fs::create_directories((root_ / relative).parent_path());
    std::ofstream(root_ / relative, std::ios::binary) << content;
}

std::string SandboxTree::read(const fs::path& relative) const
{
    std::ostringstream content;
    content << std::ifstream(root_ / relative, std::ios::binary).rdbuf();
    return content.str();
}

bool SandboxTree::exists(const fs::path& relative) const
{
    return fs::exists(root_ / relative);
}

std::vector<std::string> SandboxTree::entries() const
{
    std::vector<std::string> entries;
    for (const auto& entry : fs::recursive_directory_iterator(root_))
        entries.push_back(entry.path().lexically_relative(root_).generic_string());
    std::sort(entries.begin(), entries.end());
    return entries;
}

}
