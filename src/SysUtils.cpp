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

class FileIterator_Dummy : public FileIterator
{
public:
    FileIterator_Dummy(const string& baseDirPath) {}
    virtual ~FileIterator_Dummy() {}
    Holder Clone() const override
    { return nullptr; }
    bool SetFilterPattern(const string& filterPattern, bool isRegexPattern) override
    { return false; }
    void SetCaseSensitive(bool sensitive) override
    { return; }
    string GetBaseDirPath() const override
    { return ""; }
    string GetNextFilePath() override
    { return ""; }
    uint32_t GetNextFileIndex() const override
    { return 0; }
    vector<string> GetAllFilePaths() override
    { return {}; }
    uint32_t GetValidFileCount(bool refresh) override
    { return 0; }
    bool SeekToValidFile(uint32_t index) override
    { return false; }
    string JoinBaseDirPath(const string& relativeFilePath) const override
    { return ""; }
    string GetError() const override
    { return ""; }
};

#if (defined(__cplusplus) && __cplusplus >= 201703L) || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
typedef FileIterator_Dummy FileIterator_Impl;
#pragma message("Need to implement 'FileIterator' in this ENV.")
#elif defined(_WIN32) && !defined(__MINGW64__)
typedef FileIterator_Dummy FileIterator_Impl;
#pragma message("Need to implement 'FileIterator' in this ENV.")
#else
class FileIterator_Impl : public FileIterator
{
public:
    FileIterator_Impl(const string& baseDirPath)
    {
        if (!baseDirPath.empty())
            m_pDIR = opendir(baseDirPath.c_str());
        else
            m_pDIR = NULL;
        if (m_pDIR)
        {
            if (baseDirPath.back() != '/' && baseDirPath.back() != '\\')
                m_baseDirPath = baseDirPath+'/';
            else
                m_baseDirPath = baseDirPath;
        }
    }

    virtual ~FileIterator_Impl()
    {
        if (m_pDIR)
        {
            closedir(m_pDIR);
            m_pDIR = NULL;
        }
    }

    Holder Clone() const override
    {
        Holder hNewIns = CreateInstance(m_baseDirPath);
        hNewIns->SetCaseSensitive(m_caseSensitive);
        hNewIns->SetFilterPattern(m_filterPattern, m_isRegexPattern);
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

    string GetBaseDirPath() const override
    {
        return m_baseDirPath;
    }

    string GetNextFilePath() override
    {
        if (m_pDIR == NULL)
        {
            m_errMsg = "DIR pointer is NULL!";
            return "";
        }
        struct dirent *ent;
        while ((ent = readdir(m_pDIR)) != NULL)
        {
            if (IsCorrectFileType(ent))
            {
                if (IsMatchPattern(ent->d_name))
                    break;
            }
        }
        if (ent == NULL)
        {
            m_errMsg = "DIR eof.";
            return "";
        }
        m_fileIndex++;
        return string(ent->d_name);
    }

    uint32_t GetNextFileIndex() const override
    {
        return m_fileIndex;
    }

    vector<string> GetAllFilePaths() override
    {
        if (m_pDIR == NULL)
        {
            m_errMsg = "DIR pointer is NULL!";
            return {};
        }
        auto dirPos = telldir(m_pDIR);
        rewinddir(m_pDIR);
        vector<string> paths;
        struct dirent *ent;
        while ((ent = readdir(m_pDIR)) != NULL)
        {
            if (IsCorrectFileType(ent))
            {
                if (IsMatchPattern(ent->d_name))
                    paths.push_back(string(ent->d_name));
            }
        }
        seekdir(m_pDIR, dirPos);
        return paths;
    }

    uint32_t GetValidFileCount(bool refresh) override
    {
        if (m_validFileCountReady && !refresh)
            return m_validFileCount;

        if (m_pDIR == NULL)
        {
            m_errMsg = "DIR pointer is NULL!";
            return 0;
        }
        auto dirPos = telldir(m_pDIR);
        rewinddir(m_pDIR);
        uint32_t cnt = 0;
        struct dirent *ent;
        while ((ent = readdir(m_pDIR)) != NULL)
        {
            if (IsCorrectFileType(ent))
            {
                if (IsMatchPattern(ent->d_name))
                    cnt++;
            }
        }
        seekdir(m_pDIR, dirPos);
        m_validFileCount = cnt;
        m_validFileCountReady = true;
        return cnt;
    }

    bool SeekToValidFile(uint32_t index) override
    {
        if (m_pDIR == NULL)
        {
            m_errMsg = "DIR pointer is NULL!";
            return false;
        }
        if (m_fileIndex == index)
            return true;
        if (index < m_fileIndex)
        {
            rewinddir(m_pDIR);
            m_fileIndex = 0;
        }
        if (index == 0)
            return true;

        struct dirent *ent;
        while (m_fileIndex < index && (ent = readdir(m_pDIR)) != NULL)
        {
            if (IsCorrectFileType(ent))
            {
                if (IsMatchPattern(ent->d_name))
                    m_fileIndex++;
            }
        }
        if (ent == NULL)
        {
            ostringstream oss; oss << "Eof before seeking to the " << index << "th valid file position.";
            m_errMsg = oss.str();
            return false;
        }
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

    bool IsCorrectFileType(struct dirent* ent)
    {
#ifdef _DIRENT_HAVE_D_TYPE
        if (ent->d_type == DT_REG)
            return true;
#endif
        struct stat fileStat;
        string fullPath = JoinBaseDirPath(ent->d_name);
        int ret;
        if ((ret = stat(fullPath.c_str(), &fileStat)) < 0)
        {
            Log(Error) << "FAILED to invoke 'stat' on file '" << ent->d_name << "' in directory '" << m_baseDirPath << "'! ret=" << ret << "." << endl;
            return false;
        }
        const auto st_mode = fileStat.st_mode;
        return (st_mode&S_IFREG)!=0 && (st_mode&S_IREAD)!=0;
    }

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

private:
    string m_baseDirPath;
    DIR* m_pDIR;
    string m_filterPattern;
    bool m_caseSensitive{true};
    bool m_isRegexPattern;
    regex m_filterRegex;
    uint32_t m_fileIndex{0};
    bool m_validFileCountReady{false};
    uint32_t m_validFileCount{0};
    string m_errMsg;
};
#endif


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