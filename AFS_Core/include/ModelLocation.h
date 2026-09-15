#pragma once
#include <filesystem>

namespace AFS_Core {

namespace model_location {  // [ModelLocation.md#model_location]
constexpr char PATH_VARIABLE[] = "AUTOFILESYNC_LLAMA_MODEL";
std::filesystem::path directory();  // [ModelLocation.md#directory]
std::filesystem::path find();  // [ModelLocation.md#find]
}

} // namespace AFS_Core
