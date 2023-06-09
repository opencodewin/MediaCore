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
#include <atomic>
#include <memory>
#include <functional>
#include <vector>
#include <list>
#include "MediaCore.h"
#include "SharedSettings.h"
#include "MediaReader.h"
#include "VideoTransformFilter.h"

namespace MediaCore
{
struct VideoClip;
struct VideoFilter
{
    using Holder = std::shared_ptr<VideoFilter>;
    virtual ~VideoFilter() {}

    virtual const std::string GetFilterName() const = 0;
    virtual Holder Clone() = 0;
    virtual void ApplyTo(VideoClip* clip) = 0;
    virtual const MediaCore::VideoClip* GetVideoClip() const = 0;
    virtual void UpdateClipRange() = 0;
    virtual ImGui::ImMat FilterImage(const ImGui::ImMat& vmat, int64_t pos) = 0;
};

struct VideoClip
{
    using Holder = std::shared_ptr<VideoClip>;
    static MEDIACORE_API Holder CreateVideoInstance(
        int64_t id, MediaParser::Holder hParser, SharedSettings::Holder hSettings,
        int64_t start, int64_t end, int64_t startOffset, int64_t endOffset, int64_t readpos, bool forward);
    static MEDIACORE_API Holder CreateImageInstance(
        int64_t id, MediaParser::Holder hParser, SharedSettings::Holder hSettings,
        int64_t start, int64_t duration);

    virtual Holder Clone(SharedSettings::Holder hSettings) const = 0;
    virtual MediaParser::Holder GetMediaParser() const = 0;
    virtual int64_t Id() const = 0;
    virtual int64_t TrackId() const = 0;
    virtual bool IsImage() const = 0;
    virtual int64_t Start() const = 0;
    virtual int64_t End() const = 0;
    virtual int64_t StartOffset() const = 0;
    virtual int64_t EndOffset() const = 0;
    virtual int64_t Duration() const = 0;
    virtual uint32_t SrcWidth() const = 0;
    virtual uint32_t SrcHeight() const = 0;
    virtual uint32_t OutWidth() const = 0;
    virtual uint32_t OutHeight() const = 0;

    virtual void SetTrackId(int64_t trackId) = 0;
    virtual void SetStart(int64_t start) = 0;
    virtual void ChangeStartOffset(int64_t startOffset) = 0;
    virtual void ChangeEndOffset(int64_t endOffset) = 0;
    virtual void SetDuration(int64_t duration) = 0;
    virtual void ReadVideoFrame(int64_t pos, std::vector<CorrelativeFrame>& frames, ImGui::ImMat& out, bool& eof) = 0;
    virtual MediaReader::VideoFrame::Holder ReadSourceFrame(int64_t pos, bool& eof, bool wait) = 0;
    virtual void ProcessSourceFrame(int64_t pos, std::vector<CorrelativeFrame>& frames, ImGui::ImMat& out, MediaReader::VideoFrame::Holder hInVf) = 0;
    virtual void SeekTo(int64_t pos) = 0;
    virtual void NotifyReadPos(int64_t pos) = 0;
    virtual void SetDirection(bool forward) = 0;
    virtual void SetFilter(VideoFilter::Holder filter) = 0;
    virtual VideoFilter::Holder GetFilter() const = 0;
    virtual VideoTransformFilterHolder GetTransformFilter() = 0;

    virtual void SetLogLevel(Logger::Level l) = 0;

    static MEDIACORE_API bool USE_HWACCEL;  // TODO: should find a better place for this global control parameter
    friend std::ostream& operator<<(std::ostream& os, VideoClip::Holder hClip);
};

struct VideoOverlap;
struct VideoTransition
{
    using Holder = std::shared_ptr<VideoTransition>;
    virtual ~VideoTransition() {}

    virtual Holder Clone() = 0;
    virtual void ApplyTo(VideoOverlap* overlap) = 0;
    virtual ImGui::ImMat MixTwoImages(const ImGui::ImMat& vmat1, const ImGui::ImMat& vmat2, int64_t pos, int64_t dur) = 0;
};

struct VideoOverlap
{
    using Holder = std::shared_ptr<VideoOverlap>;
    static MEDIACORE_API bool HasOverlap(VideoClip::Holder hClip1, VideoClip::Holder hClip2);
    static MEDIACORE_API Holder CreateInstance(int64_t id, VideoClip::Holder hClip1, VideoClip::Holder hClip2);

    virtual int64_t Id() const = 0;
    virtual void SetId(int64_t id) = 0;
    virtual int64_t Start() const = 0;
    virtual int64_t End() const = 0;
    virtual int64_t Duration() const = 0;
    virtual VideoClip::Holder FrontClip() const = 0;
    virtual VideoClip::Holder RearClip() const = 0;

    virtual void ReadVideoFrame(int64_t pos, std::vector<CorrelativeFrame>& frames, ImGui::ImMat& out, bool& eof) = 0;
    virtual void SetTransition(VideoTransition::Holder hTrans) = 0;
    virtual void ProcessSourceFrame(int64_t pos, std::vector<CorrelativeFrame>& frames, ImGui::ImMat& out, MediaReader::VideoFrame::Holder hInVf1, MediaReader::VideoFrame::Holder hInVf2) = 0;
    virtual void SeekTo(int64_t pos) = 0;
    virtual void Update() = 0;
    virtual VideoTransition::Holder GetTransition() const = 0;

    friend std::ostream& operator<<(std::ostream& os, const Holder& hOverlap);
};
}
