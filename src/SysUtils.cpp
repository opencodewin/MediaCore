/*
    Copyright (c) 2023 CodeWin

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <sstream>
#include <cstdio>
#include <regex>
#include <vector>
#include <list>
#include <atomic>
#include <thread>
#include <chrono>
#include "SysUtils.h"
#include "Logger.h"
#if defined(_WIN32) && !defined(__MINGW64__)
#include <windows.h>
#else
#include <pthread.h>
#endif

#if (defined(__cplusplus) && __cplusplus >= 201703L) || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
#include <filesystem>
namespace fs = std::filesystem;
#elif defined(_WIN32) && !defined(__MINGW64__)
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#endif

using namespace std;
using namespace Logger;

namespace SysUtils
{
void SetThreadName(thread& t, const string& name)
{
#if defined(_WIN32) && !defined(__MINGW64__)
    DWORD threadId = ::GetThreadId(static_cast<HANDLE>(t.native_handle()));
    SetThreadName(threadId, name.c_str());
#elif defined(__APPLE__)
    /*
    // Apple pthread_setname_np only set current thread name and 
    // No other API can set thread name from other thread
    if (name.length() > 15)
    {
        string shortName = name.substr(0, 15);
        pthread_setname_np(shortName.c_str());
    }
    else
    {
        pthread_setname_np(name.c_str());
    }
    */
#else
    auto handle = t.native_handle();
    if (name.length() > 15)
    {
        string shortName = name.substr(0, 15);
        pthread_setname_np(handle, shortName.c_str());
    }
    else
    {
        pthread_setname_np(handle, name.c_str());
    }
#endif
}

#if defined(_WIN32)
static const char _PATH_SEPARATOR = '\\';
#else
static const char _PATH_SEPARATOR = '/';
#endif
static const char _FILE_EXT_SEPARATOR = '.';

string ExtractFileBaseName(const string& path)
{
    auto lastSlashPos = path.rfind(_PATH_SEPARATOR);
    auto lastDotPos = path.rfind(_FILE_EXT_SEPARATOR);
    if (lastSlashPos == path.length()-1)
    {
        return "";
    }
    else if (lastSlashPos == string::npos)
    {
        if (lastDotPos == string::npos || lastDotPos == 0)
            return path;
        else
            return path.substr(0, lastDotPos);
    }
    else
    {
        if (lastDotPos == string::npos || lastDotPos <= lastSlashPos+1)
            return path.substr(lastSlashPos+1);
        else
            return path.substr(lastSlashPos+1, lastDotPos-lastSlashPos-1);
    }
}

string ExtractFileExtName(const string& path)
{
    auto lastSlashPos = path.rfind(_PATH_SEPARATOR);
    auto lastDotPos = path.rfind(_FILE_EXT_SEPARATOR);
    if (lastSlashPos == path.length()-1)
    {
        return "";
    }
    else if (lastSlashPos == string::npos)
    {
        if (lastDotPos == string::npos || lastDotPos == 0)
            return "";
        else
            return path.substr(lastDotPos);
    }
    else
    {
        if (lastDotPos == string::npos || lastDotPos <= lastSlashPos+1)
            return "";
        else
            return path.substr(lastDotPos);
    }
}

string ExtractFileName(const string& path)
{
    auto lastSlashPos = path.rfind(_PATH_SEPARATOR);
    if (lastSlashPos == path.length()-1)
    {
        return "";
    }
    else if (lastSlashPos == string::npos)
    {
        return path;
    }
    else
    {
        return path.substr(lastSlashPos+1);
    }
}

string ExtractDirectoryPath(const string& path)
{
    auto lastSlashPos = path.rfind(_PATH_SEPARATOR);
    if (lastSlashPos == path.length()-1)
    {
        return path;
    }
    else if (lastSlashPos == string::npos)
    {
        return "";
    }
    else
    {
        return path.substr(0, lastSlashPos+1);
    }
}

bool IsDirectory(const string& path)
{
#if (defined(__cplusplus) && __cplusplus >= 201703L) || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
    return fs::is_directory(path);
#else
    bool isDir = false;
    DIR* pDir = opendir(path.c_str());
    if (pDir)
    {
        isDir = true;
        closedir(pDir);
    }
    return isDir;
#endif
}

class FileIterator_Impl : public FileIterator
{
public:
    FileIterator_Impl(const string& baseDirPath)
    {
        if (baseDirPath.back() != _PATH_SEPARATOR)
            m_baseDirPath = baseDirPath+_PATH_SEPARATOR;
        else
            m_baseDirPath = baseDirPath;
    }

    virtual ~FileIterator_Impl()
    {
        if (m_parseThread.joinable())
        {
            m_quitThread = true;
            m_parseThread.join();
        }
    }

    Holder Clone() const override
    {
        Holder hNewIns = CreateInstance(m_baseDirPath);
        hNewIns->SetCaseSensitive(m_caseSensitive);
        hNewIns->SetFilterPattern(m_filterPattern, m_isRegexPattern);
        hNewIns->SetRecursive(m_isRecursive);
        if (m_isQuickSampleReady)
        {
            FileIterator_Impl* pFileIter = dynamic_cast<FileIterator_Impl*>(hNewIns.get());
            pFileIter->m_quickSample = m_quickSample;
            pFileIter->m_isQuickSampleReady = true;
        }
        if (m_isParsed && !m_parseFailed)
        {
            FileIterator_Impl* pFileIter = dynamic_cast<FileIterator_Impl*>(hNewIns.get());
            pFileIter->m_paths = m_paths;
            pFileIter->m_isParsed = true;
        }
        else
        {
            hNewIns->StartParsing();
        }
        return hNewIns;
    }

    bool SetFilterPattern(const string& filterPattern, bool isRegexPattern) override
    {
        if (filterPattern == m_filterPattern && isRegexPattern == m_isRegexPattern)
            return true;
        m_filterPattern = filterPattern;
        m_isRegexPattern = isRegexPattern;
        if (isRegexPattern)
        {
            regex::flag_type flags = m_caseSensitive ? regex::optimize : regex::icase|regex::optimize;
            m_filterRegex = regex(filterPattern, flags);
        }
        return true;
    }

    void SetCaseSensitive(bool sensitive) override
    {
        if (sensitive == m_caseSensitive)
            return;
        m_caseSensitive = sensitive;
        if (m_isRegexPattern)
        {
            regex::flag_type flags = m_caseSensitive ? regex::optimize : regex::icase|regex::optimize;
            m_filterRegex = regex(m_filterPattern, flags);
        }
    }

    void SetRecursive(bool recursive) override
    {
        m_isRecursive = recursive;
    }

    void StartParsing() override
    {
        StartParseThread();
    }

    std::string GetQuickSample() override
    {
        if (!m_isQuickSampleReady && !m_isParsed)
        {
            StartParseThread();
            while (!m_isQuickSampleReady && !m_isParsed)
                this_thread::sleep_for(chrono::milliseconds(5));
        }
        return m_quickSample;
    }

    string GetBaseDirPath() const override
    {
        return m_baseDirPath;
    }

    string GetCurrFilePath() override
    {
        if (!m_isParsed)
        {
            StartParseThread();
            while (!m_isParsed)
                this_thread::sleep_for(chrono::milliseconds(5));
        }
        if (m_fileIndex >= m_paths.size())
        {
            m_errMsg = "End of path list.";
            return "";
        }
        return m_paths[m_fileIndex];
    }

    string GetNextFilePath() override
    {
        if (!m_isParsed)
        {
            StartParseThread();
            while (!m_isParsed)
                this_thread::sleep_for(chrono::milliseconds(5));
        }
        if (m_fileIndex+1 >= m_paths.size())
        {
            m_errMsg = "End of path list.";
            return "";
        }
        m_fileIndex++;
        return m_paths[m_fileIndex];
    }

    uint32_t GetCurrFileIndex() const override
    {
        return m_fileIndex;
    }

    vector<string> GetAllFilePaths() override
    {
        if (!m_isParsed)
        {
            StartParseThread();
            while (!m_isParsed)
                this_thread::sleep_for(chrono::milliseconds(5));
        }
        return vector<string>(m_paths);
    }

    uint32_t GetValidFileCount(bool refresh) override
    {
        if (!m_isParsed)
        {
            StartParseThread();
            while (!m_isParsed)
                this_thread::sleep_for(chrono::milliseconds(5));
        }
        return m_paths.size();
    }

    bool SeekToValidFile(uint32_t index) override
    {
        if (!m_isParsed)
        {
            StartParseThread();
            while (!m_isParsed)
                this_thread::sleep_for(chrono::milliseconds(5));
        }
        if (index >= m_paths.size())
        {
            m_errMsg = "Arugment 'index' is out of valid range!";
            return false;
        }
        m_fileIndex = index;
        return true;
    }

    string JoinBaseDirPath(const string& relativeFilePath) const override
    {
        ostringstream oss; oss << m_baseDirPath << relativeFilePath;
        return oss.str();
    }

    string JoinBaseDirPath(const char* pPath) const
    {
        ostringstream oss; oss << m_baseDirPath << pPath;
        return oss.str();
    }

    string GetError() const override
    {
        return m_errMsg;
    }

#if (defined(__cplusplus) && __cplusplus >= 201703L) || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
    bool IsCorrectFileType(const fs::directory_entry& dirEntry)
    {
        if (dirEntry.is_regular_file())
            return true;
        if (dirEntry.is_syslink() && dirEntry.status().type() == fs::file_type::regular)
            return true;
        return false;
    }

    bool IsSubDirectory(const fs::directory_entry& dirEntry)
    {
        return dirEntry.is_directory();
    }
#elif defined(_WIN32) && !defined(__MINGW64__)
#else
    bool IsCorrectFileType(const struct dirent* ent, const string& relativePath)
    {
#ifdef _DIRENT_HAVE_D_TYPE
        if (ent->d_type == DT_REG)
            return true;
#endif
        struct stat fileStat;
        const string fullPath = JoinBaseDirPath(relativePath);
        int ret;
        if ((ret = stat(fullPath.c_str(), &fileStat)) < 0)
        {
            Log(Error) << "FAILED to invoke 'stat' on file '" << relativePath << "' in directory '" << m_baseDirPath << "'! ret=" << ret << "." << endl;
            return false;
        }
        const auto st_mode = fileStat.st_mode;
        return (st_mode&S_IFREG)!=0 && (st_mode&S_IREAD)!=0;
    }

    bool IsSubDirectory(const struct dirent* ent, const string& relativePath)
    {
#ifdef _DIRENT_HAVE_D_TYPE
        if (ent->d_type == DT_DIR)
            return true;
#endif
        struct stat fileStat;
        const string fullPath = JoinBaseDirPath(relativePath);
        int ret;
        if ((ret = stat(fullPath.c_str(), &fileStat)) < 0)
        {
            Log(Error) << "FAILED to invoke 'stat' on file '" << relativePath << "' in directory '" << m_baseDirPath << "'! ret=" << ret << "." << endl;
            return false;
        }
        const auto st_mode = fileStat.st_mode;
        return (st_mode&S_IFDIR)!=0;
    }
#endif

    bool IsMatchPattern(const char* pPath)
    {
        if (m_isRegexPattern)
        {
            return regex_match(pPath, m_filterRegex);
        }
        else
        {
            char buff[256];
            if (sscanf(pPath, m_filterPattern.c_str(), buff) > 0)
                return true;
        }
        return false;
    }

    void StartParseThread()
    {
        bool testVal = false;
        if (m_parsingStarted.compare_exchange_strong(testVal, true))
            return;
        m_quitThread = false;
        m_parseThread = thread(&FileIterator_Impl::ParseProc, this);
    }

    void ParseProc()
    {
        list<string> pathList;
        if (!ParseOneDir("", pathList))
        {
            m_isParsed = true;
            m_parseFailed = true;
            return;
        }
        pathList.sort();
        m_paths.clear();
        m_paths.reserve(pathList.size());
        while (!pathList.empty())
        {
            m_paths.push_back(std::move(pathList.front()));
            pathList.pop_front();
        }
        m_isParsed = true;
    }

    bool ParseOneDir(const string& subDirPath, list<string>& pathList)
    {
        bool ret = true;
#if (defined(__cplusplus) && __cplusplus >= 201703L) || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
        fs::path dirFullPath = m_baseDirPath/subDirPath;
        fs::directory_iterator dirIter(dirFullPath);
        for (auto const& dirEntry : dirIter)
        {
            if (m_quitThread)
                break;
            if (m_isRecursive && IsSubDirectory(dirEntry))
            {
                const fs::path subDirPath2 = subDirPath/dirEntry.path();
                if (!ParseOneDir(subDirPath2.string(), pathList))
                {
                    ret = false;
                    break;
                }
            }
            else if (IsCorrectFileType(dirEntry) && IsMatchPattern(dirEntry.path().string()))
            {
                const fs::path filePath = subDirPath/dirEntry.path();
                pathList.push_back(filePath.string());
            }
        }
#elif defined(_WIN32) && !defined(__MINGW64__)
#else
        string dirFullPath = JoinBaseDirPath(subDirPath);
        DIR* pSubDir = opendir(dirFullPath.c_str());
        if (!pSubDir)
        {
            ostringstream oss; oss << "FAILED to open directory '" << dirFullPath << "'!";
            m_errMsg = oss.str();
            return false;
        }
        struct dirent *ent;
        while ((ent = readdir(pSubDir)) != NULL)
        {
            if (m_quitThread)
                break;
            ostringstream pathOss; pathOss << subDirPath << _PATH_SEPARATOR << ent->d_name;
            const string relativePath = pathOss.str();
            if (m_isRecursive && IsSubDirectory(ent, relativePath))
            {
                if (!ParseOneDir(pathOss.str(), pathList))
                {
                    ret = false;
                    break;
                }
            }
            else if (IsCorrectFileType(ent, relativePath) && IsMatchPattern(ent->d_name))
            {
                ostringstream pathOss; pathOss << subDirPath << _PATH_SEPARATOR << ent->d_name;
                if (pathList.empty())
                {
                    m_quickSample = pathOss.str();
                    m_isQuickSampleReady = true;
                }
                pathList.push_back(pathOss.str());
            }
        }
        closedir(pSubDir);
#endif
        return ret;
    }

private:
    string m_baseDirPath;
    bool m_isParsed{false};
    bool m_parseFailed{false};
    bool m_quitThread{false};
    atomic_bool m_parsingStarted{false};
    thread m_parseThread;
    vector<string> m_paths;
    string m_quickSample;
    bool m_isQuickSampleReady{false};
    bool m_isRecursive{false};
    string m_filterPattern;
    bool m_caseSensitive{true};
    bool m_isRegexPattern;
    regex m_filterRegex;
    uint32_t m_fileIndex{0};
    string m_errMsg;
};

static const auto FILE_ITERATOR_HOLDER_DELETER = [] (FileIterator* p) {
    FileIterator_Impl* ptr = dynamic_cast<FileIterator_Impl*>(p);
    delete ptr;
};

FileIterator::Holder FileIterator::CreateInstance(const string& baseDirPath)
{
    if (!IsDirectory(baseDirPath))
        return nullptr;
    return FileIterator::Holder(new FileIterator_Impl(baseDirPath), FILE_ITERATOR_HOLDER_DELETER);
}
}