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
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include "immat.h"
#include "MediaCore.h"
#include "MediaParser.h"
#include "Logger.h"

namespace MediaCore
{
struct MediaReader
{
    using Holder = std::shared_ptr<MediaReader>;
    static MEDIACORE_API Holder CreateInstance(const std::string& loggerName = "");
    static MEDIACORE_API Holder CreateVideoInstance(const std::string& loggerName = "");
    static MEDIACORE_API Logger::ALogger* GetDefaultLogger();
    static MEDIACORE_API Logger::ALogger* GetVideoLogger();

    virtual bool Open(const std::string& url) = 0;
    virtual bool Open(MediaParser::Holder hParser) = 0;
    virtual bool ConfigVideoReader(
            uint32_t outWidth, uint32_t outHeight,
            ImColorFormat outClrfmt = IM_CF_RGBA, ImInterpolateMode rszInterp = IM_INTERPOLATE_BICUBIC) = 0;
    virtual bool ConfigVideoReader(
            float outWidthFactor, float outHeightFactor,
            ImColorFormat outClrfmt = IM_CF_RGBA, ImInterpolateMode rszInterp = IM_INTERPOLATE_BICUBIC) = 0;
    virtual bool ConfigAudioReader(uint32_t outChannels, uint32_t outSampleRate, const std::string& outPcmFormat = "fltp", uint32_t audioStreamIndex = 0) = 0;
    virtual bool Start(bool suspend = false) = 0;
    virtual bool Stop() = 0;
    virtual void Close() = 0;
    virtual bool SeekTo(double pos) = 0;
    virtual void SetDirection(bool forward) = 0;
    virtual void Suspend() = 0;
    virtual void Wakeup() = 0;

    virtual bool ReadVideoFrame(double pos, ImGui::ImMat& m, bool& eof, bool wait = true) = 0;
    virtual bool ReadAudioSamples(uint8_t* buf, uint32_t& size, double& pos, bool& eof, bool wait = true) = 0;
    virtual bool ReadAudioSamples(ImGui::ImMat& m, uint32_t readSamples, bool& eof, bool wait = true) = 0;

    virtual bool IsOpened() const = 0;
    virtual bool IsStarted() const = 0;
    virtual MediaParser::Holder GetMediaParser() const = 0;
    virtual bool IsVideoReader() const = 0;
    virtual bool IsDirectionForward() const = 0;
    virtual bool IsSuspended() const = 0;
    virtual bool IsPlanar() const = 0;

    virtual bool SetCacheDuration(double forwardDur, double backwardDur) = 0;
    virtual std::pair<double, double> GetCacheDuration() const = 0;
    virtual bool IsHwAccelEnabled() const = 0;
    virtual void EnableHwAccel(bool enable) = 0;

    virtual MediaInfo::Holder GetMediaInfo() const = 0;
    virtual const VideoStream* GetVideoStream() const = 0;
    virtual const AudioStream* GetAudioStream() const = 0;
    virtual uint32_t GetVideoOutWidth() const = 0;
    virtual uint32_t GetVideoOutHeight() const = 0;
    virtual std::string GetAudioOutPcmFormat() const = 0;
    virtual uint32_t GetAudioOutChannels() const = 0;
    virtual uint32_t GetAudioOutSampleRate() const = 0;
    virtual uint32_t GetAudioOutFrameSize() const = 0;

    virtual void SetLogLevel(Logger::Level l) = 0;
    virtual std::string GetError() const = 0;
};
}
