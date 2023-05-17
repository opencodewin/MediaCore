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

#pragma
#include <functional>
#include <mutex>
#include <list>
#include "VideoClip.h"
#include "MediaCore.h"

namespace MediaCore
{
struct VideoTrack
{
    using Holder = std::shared_ptr<VideoTrack>;
    static MEDIACORE_API Holder CreateInstance(int64_t id, uint32_t outWidth, uint32_t outHeight, const Ratio& frameRate);
    virtual Holder Clone(uint32_t outWidth, uint32_t outHeight, const Ratio& frameRate) = 0;

    virtual int64_t Id() const = 0;
    virtual uint32_t OutWidth() const = 0;
    virtual uint32_t OutHeight() const = 0;
    virtual Ratio FrameRate() const = 0;
    virtual int64_t Duration() const = 0;
    virtual int64_t ReadPos() const = 0;
    virtual void SetDirection(bool forward) = 0;
    virtual bool Direction() const = 0;
    virtual void SetVisible(bool visible) = 0;
    virtual bool IsVisible() const = 0;
    virtual void ReadVideoFrame(std::vector<CorrelativeFrame>& frames, ImGui::ImMat& out) = 0;
    virtual void SeekTo(int64_t pos) = 0;
    virtual void SetReadFrameIndex(int64_t index) = 0;
    virtual void SkipOneFrame() = 0;

    virtual VideoClip::Holder AddNewClip(int64_t clipId, MediaParser::Holder hParser, int64_t start, int64_t startOffset, int64_t endOffset, int64_t readPos) = 0;
    virtual void InsertClip(VideoClip::Holder hClip) = 0;
    virtual void MoveClip(int64_t id, int64_t start) = 0;
    virtual void ChangeClipRange(int64_t id, int64_t startOffset, int64_t endOffset) = 0;
    virtual VideoClip::Holder RemoveClipById(int64_t clipId) = 0;
    virtual VideoClip::Holder RemoveClipByIndex(uint32_t index) = 0;
    virtual VideoClip::Holder GetClipByIndex(uint32_t index) = 0;
    virtual VideoClip::Holder GetClipById(int64_t id) = 0;
    virtual VideoOverlap::Holder GetOverlapById(int64_t id) = 0;
    virtual void UpdateClipState() = 0;

    virtual uint32_t ClipCount() const = 0;
    virtual std::list<VideoClip::Holder>::iterator ClipListBegin() = 0;
    virtual std::list<VideoClip::Holder>::iterator ClipListEnd() = 0;
    virtual uint32_t OverlapCount() const = 0;
    virtual std::list<VideoOverlap::Holder>::iterator OverlapListBegin() = 0;
    virtual std::list<VideoOverlap::Holder>::iterator OverlapListEnd() = 0;
};

MEDIACORE_API std::ostream& operator<<(std::ostream& os, VideoTrack::Holder hTrack);
}