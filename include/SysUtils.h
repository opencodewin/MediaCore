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

#pragma once
#include <thread>
#include <string>
#include <memory>
#include <cstdint>
#include <vector>
#include "MediaCore.h"

namespace SysUtils
{
MEDIACORE_API void SetThreadName(std::thread& t, const std::string& name);
MEDIACORE_API std::string ExtractFileBaseName(const std::string& path);
MEDIACORE_API std::string ExtractFileExtName(const std::string& path);
MEDIACORE_API std::string ExtractFileName(const std::string& path);
MEDIACORE_API std::string ExtractDirectoryPath(const std::string& path);
MEDIACORE_API bool IsDirectory(const std::string& path);

struct FileIterator
{
    using Holder = std::shared_ptr<FileIterator>;
    static Holder CreateInstance(const std::string& baseDirPath);
    virtual Holder Clone() const = 0;

    virtual bool SetFilterPattern(const std::string& filterPattern, bool isRegexPattern) = 0;
    virtual void SetCaseSensitive(bool sensitive) = 0;
    virtual std::string GetBaseDirPath() const = 0;
    virtual std::string GetNextFilePath() = 0;
    virtual uint32_t GetNextFileIndex() const = 0;
    virtual std::vector<std::string> GetAllFilePaths() = 0;
    virtual uint32_t GetValidFileCount(bool refresh = false) = 0;
    virtual bool SeekToValidFile(uint32_t index) = 0;
    virtual std::string JoinBaseDirPath(const std::string& relativeFilePath) const = 0;

    virtual std::string GetError() const = 0;
};
}