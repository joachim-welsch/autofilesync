#include "pch.h"
#include "DirUtil.h"
#include "FileUtil.h"
#include "SyncFileSsh.h"
#include "Util.h"
#include <filesystem>

using namespace AFS_Core;

namespace tu {

    bool erase_test_dir(const std::wstring& fn, bool display_error)
    {
        if (!std::filesystem::exists(fn)) {
            return true; // directory does not exist, nothing to delete
        }

        try {
            for (const auto& entry : std::filesystem::directory_iterator(fn)) {
                if (entry.is_directory()) {
                    erase_test_dir(entry.path().wstring(), display_error);
                }
                else {
                    std::filesystem::remove(entry.path());
                }
            }
            std::filesystem::remove_all(fn);
            return true;
        }
        catch (const std::filesystem::filesystem_error& e) {
            if (display_error)
                std::wcerr << L"Fehler beim Loeschen des Verzeichnisses: " << fn << L"\n" << e.what() << std::endl;
            return false;
        }
    }

    void erase_ssh_test_dir(const std::wstring& ssh_path)
    {
        if (!ssh_exists(ssh_path) || !ssh_is_directory(ssh_path))
            return;
        for (const auto& entry : ssh_list_dir(ssh_path)) {
            const std::wstring child = util::with_trailing_char(ssh_path, L'/') + entry.name;
            if (entry.is_regular_file)
                ssh_remove_file(child);
            else if (entry.is_directory)
                erase_ssh_test_dir(child); // no remote rmdir available; leaving the now-empty directory behind is harmless
        }
    }

    void erase_ssh_test_dir_variants(const std::wstring& ssh_base_path, const std::vector<std::wstring>& suffixes)
    {
        erase_ssh_test_dir(ssh_base_path);
        for (const auto& suffix : suffixes)
            erase_ssh_test_dir(ssh_base_path + suffix);
    }

    bool check_copied_files(const std::wstring& source_dir, const std::wstring& dest_dir)
    {
        for (const auto& entry : std::filesystem::directory_iterator(source_dir)) {
            auto source_file = entry.path();
            std::filesystem::path dest_path(dest_dir);
            auto dest_file = dest_path / source_file.filename();

            if (std::filesystem::is_directory(source_file)) {
                continue;
            }

            if (!std::filesystem::exists(dest_file)) {
                return false; // File not copied
            }
            if (std::filesystem::file_size(source_file) != std::filesystem::file_size(dest_file)) {
                return false; // File size is different
            }
        }
        return true; // All files copied successfully
    }

    std::string concat_path(const std::string& path1, const std::string& path2)
    {
#ifdef _UNIX
		const char separator = '/';
#else
		const char separator = '\\';
#endif
        if (path1.empty()) return path2;
        if (path2.empty()) return path1;
        if (path1.back() == separator) {
            if (path2.front() == separator) {
                return path1 + path2.substr(1); // Remove leading separator from path2
            }
            else {
                return path1 + path2;
            }
        }
        else {
            if (path2.front() == separator) {
                return path1 + path2;
            }
            else {
                return path1 + separator + path2;
            }
		}
    }

        std::wstring concat_path(const std::wstring& path1, const std::wstring& path2)
        {
    #ifdef _UNIX
            const wchar_t separator = L'/';
    #else
            const wchar_t separator = L'\\';
    #endif
            if (path1.empty()) return path2;
            if (path2.empty()) return path1;
            if (path1.back() == separator) {
                if (path2.front() == separator) {
                    return path1 + path2.substr(1); // Remove leading separator from path2
                }
                else {
                    return path1 + path2;
                }
            }
            else {
                if (path2.front() == separator) {
                    return path1 + path2;
                }
                else {
                    return path1 + separator + path2;
                }
            }
        }

    std::string concat_temp(const std::string& path)
    {
        return concat_path(temp_dir(), path);
    }

    std::wstring concat_temp(const std::wstring& path)
    {
        return concat_path(temp_dir_w(), path);
    }
#ifdef _UNIX
    std::wstring concat_ssh_temp(const std::wstring& path)
    {
        return concat_path(L"/tmp", path);
	}
#else
    std::wstring concat_ssh_temp(const std::wstring& path)
    {
        return concat_path(L"\\tmp", path);
	}
#endif
#ifdef _UNIX
    std::string concat_ssh_temp(const std::string& path)
    {
        return concat_path("/tmp", path);
    }
#else
    std::string concat_ssh_temp(const std::string& path)
    {
        return concat_path("\\tmp", path);
    }
#endif
} // namespace tu