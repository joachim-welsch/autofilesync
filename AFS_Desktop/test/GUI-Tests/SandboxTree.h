#pragma once

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace guitest {

using FileContents = std::vector<std::pair<std::filesystem::path, std::string>>;  // relative path -> content; a trailing separator makes a directory

class SandboxTree {  // [SandboxTree.md#SandboxTree]
public:
    SandboxTree(std::filesystem::path root, const FileContents& files = {});
    ~SandboxTree();
    SandboxTree(const SandboxTree&) = delete;
    SandboxTree& operator=(const SandboxTree&) = delete;
    const std::filesystem::path& root() const { return root_; }
    std::filesystem::path path(const std::filesystem::path& relative) const { return root_ / relative; }
    void write(const std::filesystem::path& relative, const std::string& content) const;
    std::string read(const std::filesystem::path& relative) const;
    bool exists(const std::filesystem::path& relative) const;
    std::vector<std::string> entries() const;  // [SandboxTree.md#entries]
private:
    std::filesystem::path root_;
};

}
