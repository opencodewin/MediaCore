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
#include <algorithm>
#include "VideoTrack.h"
#include "MediaCore.h"
#include "DebugHelper.h"

using namespace std;

namespace MediaCore
{
static const auto CLIP_SORT_CMP = [] (const VideoClip::Holder& a, const VideoClip::Holder& b){
    return a->Start() < b->Start();
};

static const auto OVERLAP_SORT_CMP = [] (const VideoOverlap::Holder& a, const VideoOverlap::Holder& b) {
    return a->Start() < b->Start();
};

class VideoTrack_Impl : public VideoTrack
{
public:
    VideoTrack_Impl(int64_t id, uint32_t outWidth, uint32_t outHeight, const Ratio& frameRate)
        : m_id(id), m_outWidth(outWidth), m_outHeight(outHeight), m_frameRate(frameRate)
    {
        m_readClipIter = m_clips.begin();
    }

    Holder Clone(uint32_t outWidth, uint32_t outHeight, const Ratio& frameRate) override;

    VideoClip::Holder AddNewClip(int64_t clipId, MediaParser::Holder hParser, int64_t start, int64_t startOffset, int64_t endOffset, int64_t readPos) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        VideoClip::Holder hClip;
        auto vidstream = hParser->GetBestVideoStream();
        if (vidstream->isImage)
            hClip = VideoClip::CreateImageInstance(clipId, hParser, m_outWidth, m_outHeight, start, startOffset);
        else
            hClip = VideoClip::CreateVideoInstance(clipId, hParser, m_outWidth, m_outHeight, m_frameRate, start, startOffset, endOffset, readPos-start, m_readForward);
        InsertClip(hClip);
        return hClip;
    }

    void InsertClip(VideoClip::Holder hClip) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!CheckClipRangeValid(hClip->Id(), hClip->Start(), hClip->End()))
            throw invalid_argument("Invalid argument for inserting clip!");

        // add this clip into clip list
        hClip->SetDirection(m_readForward);
        m_clips.push_back(hClip);
        hClip->SetTrackId(m_id);
        m_clips.sort(CLIP_SORT_CMP);
        // update track duration
        VideoClip::Holder lastClip = m_clips.back();
        m_duration = lastClip->Start()+lastClip->Duration();
        // update overlap
        UpdateClipOverlap(hClip);
    }

    void MoveClip(int64_t id, int64_t start) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        VideoClip::Holder hClip = GetClipById(id);
        if (!hClip)
            throw invalid_argument("Invalid value for argument 'id'!");

        if (hClip->Start() == start)
            return;
        else
            hClip->SetStart(start);

        if (!CheckClipRangeValid(id, hClip->Start(), hClip->End()))
            throw invalid_argument("Invalid argument for moving clip!");

        // update clip order
        m_clips.sort(CLIP_SORT_CMP);
        // update track duration
        VideoClip::Holder lastClip = m_clips.back();
        m_duration = lastClip->Start()+lastClip->Duration();
        // update overlap
        UpdateClipOverlap(hClip);
        // call 'SeekTo()' to update iterators
        const int64_t readPos = m_readFrames*1000*m_frameRate.den/m_frameRate.num;
        SeekTo(readPos);
    }

    void ChangeClipRange(int64_t id, int64_t startOffset, int64_t endOffset) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        VideoClip::Holder hClip = GetClipById(id);
        if (!hClip)
            throw invalid_argument("Invalid value for argument 'id'!");

        bool rangeChanged = false;
        if (hClip->IsImage())
        {
            int64_t start = startOffset>endOffset ? endOffset : startOffset;
            int64_t end = startOffset>endOffset ? startOffset : endOffset;
            if (start != hClip->Start())
            {
                hClip->SetStart(start);
                rangeChanged = true;
            }
            int64_t duration = end-start;
            if (duration != hClip->Duration())
            {
                hClip->SetDuration(duration);
                rangeChanged = true;
            }
        }
        else
        {
            if (startOffset != hClip->StartOffset())
            {
                int64_t bias = startOffset-hClip->StartOffset();
                hClip->ChangeStartOffset(startOffset);
                hClip->SetStart(hClip->Start()+bias);
                rangeChanged = true;
            }
            if (endOffset != hClip->EndOffset())
            {
                hClip->ChangeEndOffset(endOffset);
                rangeChanged = true;
            }
        }
        if (!rangeChanged)
            return;

        if (!CheckClipRangeValid(id, hClip->Start(), hClip->End()))
            throw invalid_argument("Invalid argument for changing clip range!");

        // update clip order
        m_clips.sort(CLIP_SORT_CMP);
        // update track duration
        VideoClip::Holder lastClip = m_clips.back();
        m_duration = lastClip->Start()+lastClip->Duration();
        // update overlap
        UpdateClipOverlap(hClip);
        // call 'SeekTo()' to update iterators
        const int64_t readPos = m_readFrames*1000*m_frameRate.den/m_frameRate.num;
        SeekTo(readPos);
    }

    VideoClip::Holder RemoveClipById(int64_t clipId) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        auto iter = find_if(m_clips.begin(), m_clips.end(), [clipId](const VideoClip::Holder& clip) {
            return clip->Id() == clipId;
        });
        if (iter == m_clips.end())
            return nullptr;

        VideoClip::Holder hClip = (*iter);
        m_clips.erase(iter);
        hClip->SetTrackId(-1);
        UpdateClipOverlap(hClip, true);
        // call 'SeekTo()' to update iterators
        const int64_t readPos = m_readFrames*1000*m_frameRate.den/m_frameRate.num;
        SeekTo(readPos);

        if (m_clips.empty())
            m_duration = 0;
        else
        {
            VideoClip::Holder lastClip = m_clips.back();
            m_duration = lastClip->Start()+lastClip->Duration();
        }
        return hClip;
    }

    VideoClip::Holder RemoveClipByIndex(uint32_t index) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (index >= m_clips.size())
            throw invalid_argument("Argument 'index' exceeds the count of clips!");

        auto iter = m_clips.begin();
        while (index > 0)
        {
            iter++; index--;
        }

        VideoClip::Holder hClip = (*iter);
        m_clips.erase(iter);
        hClip->SetTrackId(-1);
        UpdateClipOverlap(hClip, true);
        // call 'SeekTo()' to update iterators
        const int64_t readPos = m_readFrames*1000*m_frameRate.den/m_frameRate.num;
        SeekTo(readPos);

        if (m_clips.empty())
            m_duration = 0;
        else
        {
            VideoClip::Holder lastClip = m_clips.back();
            m_duration = lastClip->Start()+lastClip->Duration();
        }
        return hClip;
    }

    uint32_t ClipCount() const override
    {
        return m_clips.size();
    }
    
    list<VideoClip::Holder>::iterator ClipListBegin() override
    {
        return m_clips.begin();
    }
    
    list<VideoClip::Holder>::iterator ClipListEnd() override
    {
        return m_clips.end();
    }
    
    uint32_t OverlapCount() const override
    {
        return m_overlaps.size();
    }
    
    list<VideoOverlap::Holder>::iterator OverlapListBegin() override
    {
        return m_overlaps.begin();
    }
    
    list<VideoOverlap::Holder>::iterator OverlapListEnd() override
    {
        return m_overlaps.end();
    }

    int64_t Id() const override
    {
        return m_id;
    }

    uint32_t OutWidth() const override
    {
        return m_outWidth;
    }

    uint32_t OutHeight() const override
    {
        return m_outHeight;
    }

    Ratio FrameRate() const override
    {
        return m_frameRate;
    }

    int64_t Duration() const override
    {
        return m_duration;
    }

    int64_t ReadPos() const override
    {
        return m_readFrames*1000*m_frameRate.den/m_frameRate.num;
    }

    bool Direction() const override
    {
        return m_readForward;
    }

    void SeekTo(int64_t pos) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (pos < 0)
            throw invalid_argument("Argument 'pos' can NOT be NEGATIVE!");

        if (m_readForward)
        {
            // update read clip iterator
            m_readClipIter = m_clips.end();
            {
                auto iter = m_clips.begin();
                while (iter != m_clips.end())
                {
                    const VideoClip::Holder& hClip = *iter;
                    int64_t clipPos = pos-hClip->Start();
                    hClip->SeekTo(clipPos);
                    if (m_readClipIter == m_clips.end() && clipPos < hClip->Duration())
                        m_readClipIter = iter;
                    iter++;
                }
            }
            // update read overlap iterator
            m_readOverlapIter = m_overlaps.end();
            {
                auto iter = m_overlaps.begin();
                while (iter != m_overlaps.end())
                {
                    const VideoOverlap::Holder& hOverlap = *iter;
                    int64_t overlapPos = pos-hOverlap->Start();
                    if (m_readOverlapIter == m_overlaps.end() && overlapPos < hOverlap->Duration())
                    {
                        m_readOverlapIter = iter;
                        break;
                    }
                    iter++;
                }
            }
        }
        else
        {
            m_readClipIter = m_clips.end();
            {
                auto riter = m_clips.rbegin();
                while (riter != m_clips.rend())
                {
                    const VideoClip::Holder& hClip = *riter;
                    int64_t clipPos = pos-hClip->Start();
                    hClip->SeekTo(clipPos);
                    if (m_readClipIter == m_clips.end() && clipPos >= 0)
                        m_readClipIter = riter.base();
                    riter++;
                }
            }
            m_readOverlapIter = m_overlaps.end();
            {
                auto riter = m_overlaps.rbegin();
                while (riter != m_overlaps.rend())
                {
                    const VideoOverlap::Holder& hOverlap = *riter;
                    int64_t overlapPos = pos-hOverlap->Start();
                    if (m_readOverlapIter == m_overlaps.end() && overlapPos >= 0)
                        m_readOverlapIter = riter.base();
                    riter++;
                }
            }
        }

        m_readFrames = (int64_t)(pos*m_frameRate.num/(m_frameRate.den*1000));
    }

    void SetReadFrameIndex(int64_t index) override
    {
        if (m_readFrames != index)
        {
            bool isMoveForward = index > m_readFrames;
            if (!m_readForward) isMoveForward = !isMoveForward;
            m_readFrames = index;
            if (!isMoveForward)
            {
                int64_t pos = index*m_frameRate.den*1000/m_frameRate.num;
                SeekTo(pos);
            }
        }
    }

    void SkipOneFrame() override
    {
        if (m_readForward)
            m_readFrames++;
        else
            m_readFrames--;
    }

    void ReadVideoFrame(vector<CorrelativeFrame>& frames, ImGui::ImMat& out) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);

        const int64_t readPos = m_readFrames*1000*m_frameRate.den/m_frameRate.num;
        for (auto& clip : m_clips)
            clip->NotifyReadPos(readPos-clip->Start());

        if (m_readForward)
        {
            // first, find the image from a overlap
            bool readFromeOverlay = false;
            while (m_readOverlapIter != m_overlaps.end() && readPos >= (*m_readOverlapIter)->Start())
            {
                auto& hOverlap = *m_readOverlapIter;
                bool eof = false;
                if (readPos < hOverlap->End())
                {
                    hOverlap->ReadVideoFrame(readPos-hOverlap->Start(), frames, out, eof);
                    readFromeOverlay = true;
                    break;
                }
                else
                    m_readOverlapIter++;
            }

            if (!readFromeOverlay)
            {
                // then try to read the image from a clip
                while (m_readClipIter != m_clips.end() && readPos >= (*m_readClipIter)->Start())
                {
                    auto& hClip = *m_readClipIter;
                    bool eof = false;
                    if (readPos < hClip->End())
                    {
                        hClip->ReadVideoFrame(readPos-hClip->Start(), frames, out, eof);
                        break;
                    }
                    else
                        m_readClipIter++;
                }
            }

            out.time_stamp = (double)readPos/1000;
            m_readFrames++;
        }
        else
        {
            if (!m_overlaps.empty())
            {
                if (m_readOverlapIter == m_overlaps.end()) m_readOverlapIter--;
                while (m_readOverlapIter != m_overlaps.begin() && readPos < (*m_readOverlapIter)->Start())
                    m_readOverlapIter--;
                auto& hOverlap = *m_readOverlapIter;
                bool eof = false;
                if (readPos >= hOverlap->Start() && readPos < hOverlap->End())
                    hOverlap->ReadVideoFrame(readPos-hOverlap->Start(), frames, out, eof);
            }

            if (out.empty() && !m_clips.empty())
            {
                if (m_readClipIter == m_clips.end()) m_readClipIter--;
                while (m_readClipIter != m_clips.begin() && readPos < (*m_readClipIter)->Start())
                    m_readClipIter--;
                auto& hClip = *m_readClipIter;
                bool eof = false;
                if (readPos >= hClip->Start() && readPos < hClip->End())
                    hClip->ReadVideoFrame(readPos-hClip->Start(), frames, out, eof);
            }

            out.time_stamp = (double)readPos/1000;
            m_readFrames--;
        }
    }

    void SetDirection(bool forward) override
    {
        if (m_readForward == forward)
            return;
        m_readForward = forward;
        for (auto& clip : m_clips)
            clip->SetDirection(forward);
    }

    void SetVisible(bool visible) override
    {
        m_visible = visible;
    }

    bool IsVisible() const override
    {
        return m_visible;
    }

    VideoClip::Holder GetClipByIndex(uint32_t index) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (index >= m_clips.size())
            return nullptr;
        auto iter = m_clips.begin();
        while (index > 0)
        {
            iter++; index--;
        }
        return *iter;
    }

    VideoClip::Holder GetClipById(int64_t id) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        auto iter = find_if(m_clips.begin(), m_clips.end(), [id] (const VideoClip::Holder& clip) {
            return clip->Id() == id;
        });
        if (iter != m_clips.end())
            return *iter;
        return nullptr;
    }

    VideoOverlap::Holder GetOverlapById(int64_t id) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        auto iter = find_if(m_overlaps.begin(), m_overlaps.end(), [id] (const VideoOverlap::Holder& ovlp) {
            return ovlp->Id() == id;
        });
        if (iter != m_overlaps.end())
            return *iter;
        return nullptr;
    }

    friend ostream& operator<<(ostream& os, VideoTrack_Impl& track);

private:
    bool CheckClipRangeValid(int64_t clipId, int64_t start, int64_t end)
    {
        for (auto& overlap : m_overlaps)
        {
            if (clipId == overlap->FrontClip()->Id() || clipId == overlap->RearClip()->Id())
                continue;
            if ((start > overlap->Start() && start < overlap->End()) ||
                (end > overlap->Start() && end < overlap->End()))
                return false;
        }
        return true;
    }

    void UpdateClipOverlap(VideoClip::Holder hUpdateClip, bool remove = false)
    {
        const int64_t id1 = hUpdateClip->Id();
        // remove invalid overlaps
        auto ovIter = m_overlaps.begin();
        while (ovIter != m_overlaps.end())
        {
            auto& hOverlap = *ovIter;
            if (hOverlap->FrontClip()->TrackId() != m_id || hOverlap->RearClip()->TrackId() != m_id)
            {
                ovIter = m_overlaps.erase(ovIter);
                continue;
            }
            if (hOverlap->FrontClip()->Id() == id1 || hOverlap->RearClip()->Id() == id1)
            {
                hOverlap->Update();
                if (hOverlap->Duration() <= 0)
                {
                    ovIter = m_overlaps.erase(ovIter);
                    continue;
                }
            }
            ovIter++;
        }
        if (!remove)
        {
            // add new overlaps
            for (auto& clip : m_clips)
            {
                if (hUpdateClip == clip)
                    continue;
                if (VideoOverlap::HasOverlap(hUpdateClip, clip))
                {
                    const int64_t id2 = clip->Id();
                    auto iter = find_if(m_overlaps.begin(), m_overlaps.end(), [id1, id2] (const VideoOverlap::Holder& overlap) {
                        const int64_t idf = overlap->FrontClip()->Id();
                        const int64_t idr = overlap->RearClip()->Id();
                        return (id1 == idf && id2 == idr) || (id1 == idr && id2 == idf);
                    });
                    if (iter == m_overlaps.end())
                    {
                        auto hOverlap = VideoOverlap::CreateInstance(0, hUpdateClip, clip);
                        m_overlaps.push_back(hOverlap);
                    }
                }
            }
        }

        // sort overlap by 'Start' time
        m_overlaps.sort(OVERLAP_SORT_CMP);
    }

private:
    recursive_mutex m_apiLock;
    int64_t m_id;
    uint32_t m_outWidth;
    uint32_t m_outHeight;
    Ratio m_frameRate;
    list<VideoClip::Holder> m_clips;
    list<VideoClip::Holder>::iterator m_readClipIter;
    list<VideoOverlap::Holder> m_overlaps;
    list<VideoOverlap::Holder>::iterator m_readOverlapIter;
    int64_t m_readFrames{0};
    int64_t m_duration{0};
    bool m_readForward{true};
    bool m_visible{true};
};

static const auto VIDEO_TRACK_HOLDER_DELETER = [] (VideoTrack* p) {
    VideoTrack_Impl* ptr = dynamic_cast<VideoTrack_Impl*>(p);
    delete ptr;
};

VideoTrack::Holder VideoTrack::CreateInstance(int64_t id, uint32_t outWidth, uint32_t outHeight, const Ratio& frameRate)
{
    return VideoTrack::Holder(new VideoTrack_Impl(id, outWidth, outHeight, frameRate), VIDEO_TRACK_HOLDER_DELETER);
}

VideoTrack::Holder VideoTrack_Impl::Clone(uint32_t outWidth, uint32_t outHeight, const Ratio& frameRate)
{
    lock_guard<recursive_mutex> lk(m_apiLock);
    VideoTrack_Impl* newInstance = new VideoTrack_Impl(m_id, outWidth, outHeight, frameRate);
    // duplicate the clips
    for (auto clip : m_clips)
    {
        auto newClip = clip->Clone(outWidth, outHeight, frameRate);
        newInstance->m_clips.push_back(newClip);
        newClip->SetTrackId(m_id);
        VideoClip::Holder lastClip = newInstance->m_clips.back();
        newInstance->m_duration = lastClip->Start()+lastClip->Duration();
        newInstance->UpdateClipOverlap(newClip);
    }
    // clone the transitions on the overlaps
    for (auto overlap : m_overlaps)
    {
        auto iter = find_if(newInstance->m_overlaps.begin(), newInstance->m_overlaps.end(), [overlap] (auto& ovlp) {
            return overlap->FrontClip()->Id() == ovlp->FrontClip()->Id() && overlap->RearClip()->Id() == ovlp->RearClip()->Id();
        });
        if (iter != newInstance->m_overlaps.end())
        {
            auto trans = overlap->GetTransition();
            if (trans)
                (*iter)->SetTransition(trans->Clone());
        }
    }
    return VideoTrack::Holder(newInstance, VIDEO_TRACK_HOLDER_DELETER);
}

ostream& operator<<(ostream& os, VideoTrack_Impl& track)
{
    os << "{ clips(" << track.m_clips.size() << "): [";
    auto clipIter = track.m_clips.begin();
    while (clipIter != track.m_clips.end())
    {
        os << *clipIter;
        clipIter++;
        if (clipIter != track.m_clips.end())
            os << ", ";
        else
            break;
    }
    os << "], overlaps(" << track.m_overlaps.size() << "): [";
    auto ovlpIter = track.m_overlaps.begin();
    while (ovlpIter != track.m_overlaps.end())
    {
        os << *ovlpIter;
        ovlpIter++;
        if (ovlpIter != track.m_overlaps.end())
            os << ", ";
        else
            break;
    }
    os << "] }";
    return os;
}

ostream& operator<<(ostream& os, VideoTrack::Holder hTrack)
{
    VideoTrack_Impl* pTrkImpl = dynamic_cast<VideoTrack_Impl*>(hTrack.get());
    os << *pTrkImpl;
    return os;
}
}
