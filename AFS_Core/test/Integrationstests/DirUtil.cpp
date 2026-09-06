#include "pch.h"
#include "DirUtil.h"
#include "FileUtil.h"
#include "SyncFileSsh.h"
#include "util.h"
#include <filesystem>

using namespace AFS_Core;

namespace tu {

    bool eraseTestDir(const std::wstring& fn, bool displayError)
    {
        // Prüfe, ob das Verzeichnis existiert
        if (!std::filesystem::exists(fn)) {
            return true; // Verzeichnis existiert nicht, nichts zu löschen
        }

        try {
            // Iteriere durch alle Dateien und Unterverzeichnisse im Verzeichnis
            for (const auto& entry : std::filesystem::directory_iterator(fn)) {
                if (entry.is_directory()) {
                    // Rekursiv Unterverzeichnisse loeschen
                    eraseTestDir(entry.path().wstring(), displayError);
                }
                else {
                    // Datei loeschen
                    std::filesystem::remove(entry.path());
                }
            }
            // Loesche das Verzeichnis selbst
            std::filesystem::remove_all(fn);
            return true;
        }
        catch (const std::filesystem::filesystem_error& e) {
            if (displayError)
                std::wcerr << L"Fehler beim Loeschen des Verzeichnisses: " << fn << L"\n" << e.what() << std::endl;
            return false;
        }
    }

    void eraseSshTestDir(const std::wstring& sshPath)
    {
        if (!sshExists(sshPath) || !sshIsDirectory(sshPath))
            return;
        for (const auto& entry : sshListDir(sshPath)) {
            const std::wstring child = util::withTrailingChar(sshPath, L'/') + entry.name;
            if (entry.isRegularFile)
                sshRemoveFile(child);
            else if (entry.isDirectory)
                eraseSshTestDir(child); // no remote rmdir available; leaving the now-empty directory behind is harmless
        }
    }

    void eraseSshTestDirVariants(const std::wstring& sshBasePath, const std::vector<std::wstring>& suffixes)
    {
        eraseSshTestDir(sshBasePath);
        for (const auto& suffix : suffixes)
            eraseSshTestDir(sshBasePath + suffix);
    }

    bool checkCopiedFiles(const std::wstring& sourceDir, const std::wstring& destDir)
    {
        for (const auto& entry : std::filesystem::directory_iterator(sourceDir)) {
            auto sourceFile = entry.path();
            std::filesystem::path destPath(destDir);
            auto destFile = destPath / sourceFile.filename();

            // Skip directories, only check regular files
            if (std::filesystem::is_directory(sourceFile)) {
                continue;
            }

            if (!std::filesystem::exists(destFile)) {
                return false; // File not copied
            }
            // Only check if file size matches
            if (std::filesystem::file_size(sourceFile) != std::filesystem::file_size(destFile)) {
                return false; // File size is different
            }
        }
        return true; // All files copied successfully
    }

    std::string concatPath(const std::string& path1, const std::string& path2)
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

        std::wstring concatPath(const std::wstring& path1, const std::wstring& path2)
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

    std::string concatTemp(const std::string& path)
    {
        return concatPath(tempDir(), path);
    }

    std::wstring concatTemp(const std::wstring& path)
    {
        return concatPath(tempDirW(), path);
    }
#ifdef _UNIX
    std::wstring concatSSHTemp(const std::wstring& path)
    {
        return concatPath(L"/tmp", path);
	}
#else
    std::wstring concatSSHTemp(const std::wstring& path)
    {
        return concatPath(L"\\tmp", path);
	}
#endif
#ifdef _UNIX
    std::string concatSSHTemp(const std::string& path)
    {
        return concatPath("/tmp", path);
    }
#else
    std::string concatSSHTemp(const std::string& path)
    {
        return concatPath("\\tmp", path);
    }
#endif
} // namespace tu