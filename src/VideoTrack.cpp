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
#include <atomic>
#include <cassert>
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
        lock_guard<recursive_mutex> lk(m_clipChangeLock);
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
        lock_guard<recursive_mutex> lk(m_clipChangeLock);
        if (!CheckClipRangeValid(hClip->Id(), hClip->Start(), hClip->End()))
            throw invalid_argument("Invalid argument for inserting clip!");

        // add this clip into clip list 2
        hClip->SetDirection(m_readForward);
        hClip->SetTrackId(m_id);
        m_clips2.push_back(hClip);
        if (hClip->End() > m_duration2)
            m_duration2 = hClip->End();
        m_clipChanged = true;
        m_syncReadPos = true;
    }

    void MoveClip(int64_t id, int64_t start) override
    {
        lock_guard<recursive_mutex> lk(m_clipChangeLock);
        VideoClip::Holder hClip = GetClipById2(id);
        if (!hClip)
            throw invalid_argument("Invalid value for argument 'id'!");
        if (hClip->Start() == start)
            return;

        bool isTailClip = hClip->End() == m_duration2;
        hClip->SetStart(start);
        if (!CheckClipRangeValid(id, hClip->Start(), hClip->End()))
            throw invalid_argument("Invalid argument for moving clip!");

        if (hClip->End() >= m_duration2)
            m_duration2 = hClip->End();
        else if (isTailClip)
        {
            int64_t newDuration = 0;
            for (auto& clip : m_clips2)
            {
                if (clip->End() > newDuration)
                    newDuration = clip->End();
            }
            m_duration2 = newDuration;
        }
        m_clipChanged = true;
        m_syncReadPos = true;
    }

    void ChangeClipRange(int64_t id, int64_t startOffset, int64_t endOffset) override
    {
        lock_guard<recursive_mutex> lk(m_clipChangeLock);
        VideoClip::Holder hClip = GetClipById2(id);
        if (!hClip)
            throw invalid_argument("Invalid value for argument 'id'!");

        bool isTailClip = hClip->End() == m_duration2;
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

        if (hClip->End() >= m_duration2)
            m_duration2 = hClip->End();
        else if (isTailClip)
        {
            int64_t newDuration = 0;
            for (auto& clip : m_clips2)
            {
                if (clip->End() > newDuration)
                    newDuration = clip->End();
            }
            m_duration2 = newDuration;
        }
        m_clipChanged = true;
        m_syncReadPos = true;
    }

    VideoClip::Holder RemoveClipById(int64_t clipId) override
    {
        lock_guard<recursive_mutex> lk(m_clipChangeLock);
        auto iter = find_if(m_clips2.begin(), m_clips2.end(), [clipId](const VideoClip::Holder& clip) {
            return clip->Id() == clipId;
        });
        if (iter == m_clips2.end())
            return nullptr;

        auto hClip = *iter;
        bool isTailClip = hClip->End() == m_duration2;
        m_clips2.erase(iter);
        hClip->SetTrackId(-1);

        if (isTailClip)
        {
            int64_t newDuration = 0;
            for (auto& clip : m_clips2)
            {
                if (clip->End() > newDuration)
                    newDuration = clip->End();
            }
            m_duration2 = newDuration;
        }
        m_clipChanged = true;
        m_syncReadPos = true;
        return hClip;
    }

    VideoClip::Holder RemoveClipByIndex(uint32_t index) override
    {
        lock_guard<recursive_mutex> lk(m_clipChangeLock);
        if (index >= m_clips2.size())
            throw invalid_argument("Argument 'index' exceeds the count of clips!");

        auto iter = m_clips2.begin();
        while (index > 0)
        {
            iter++; index--;
        }

        auto hClip = *iter;
        bool isTailClip = hClip->End() == m_duration2;
        m_clips2.erase(iter);
        hClip->SetTrackId(-1);

        if (isTailClip)
        {
            int64_t newDuration = 0;
            for (auto& clip : m_clips2)
            {
                if (clip->End() > newDuration)
                    newDuration = clip->End();
            }
            m_duration2 = newDuration;
        }
        m_clipChanged = true;
        m_syncReadPos = true;
        return hClip;
    }

    uint32_t ClipCount() const override
    {
        if (m_clipChanged)
            return m_clips2.size();
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
        if (m_clipChanged)
            return m_duration2;
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
        const int64_t readPos = ReadPos();

        // if clip changed, update m_clips
        if (m_clipChanged)
            UpdateClipState();
        // sync read position if needed
        if (m_syncReadPos.exchange(false))
            SeekTo(readPos);
        // notify read position to each clip
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

    void UpdateClipState() override
    {
        if (!m_clipChanged)
            return;
        {
            lock_guard<recursive_mutex> lk2(m_clipChangeLock);
            if (!m_clipChanged)
                return;
            m_clips = m_clips2;
            m_clipChanged = false;
        }
        // udpate duration
        if (m_clips.empty())
        {
            m_duration = 0;
        }
        else
        {
            m_clips.sort(CLIP_SORT_CMP);
            auto& tail = m_clips.back();
            m_duration = tail->End();
        }
        // update overlap
        UpdateClipOverlap();
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

    void UpdateClipOverlap()
    {
        lock_guard<mutex> lk(m_ovlpUpdateLock);
        if (m_clips.empty())
        {
            m_overlaps.clear();
            return;
        }

        list<VideoOverlap::Holder> newOverlaps;
        auto clipIter1 = m_clips.begin();
        auto clipIter2 = clipIter1; clipIter2++;
        while (clipIter2 != m_clips.end())
        {
            const auto& clip1 = *clipIter1;
            const auto& clip2 = *clipIter2++;
            const auto cid1 = clip1->Id();
            const auto cid2 = clip2->Id();
            if (VideoOverlap::HasOverlap(clip1, clip2))
            {
                auto ovlpIter = m_overlaps.begin();
                while (ovlpIter != m_overlaps.end())
                {
                    const auto& ovlp = *ovlpIter;
                    const auto fid = ovlp->FrontClip()->Id();
                    const auto rid = ovlp->RearClip()->Id();
                    if (cid1 != fid && cid1 != rid || cid2 != fid && cid2 != rid)
                    {
                        ovlpIter++;
                        continue;
                    }
                    break;
                }
                if (ovlpIter != m_overlaps.end())
                {
                    auto& ovlp = *ovlpIter;
                    ovlp->Update();
                    assert(ovlp->Duration() > 0);
                    newOverlaps.push_back(*ovlpIter);
                }
                else
                {
                    newOverlaps.push_back(VideoOverlap::CreateInstance(0, clip1, clip2));
                }
            }
            if (clipIter2 == m_clips.end())
            {
                clipIter1++;
                clipIter2 = clipIter1;
                clipIter2++;
            }
        }

        m_overlaps = std::move(newOverlaps);
        m_overlaps.sort(OVERLAP_SORT_CMP);
    }

    VideoClip::Holder GetClipById2(int64_t id)
    {
        auto iter = find_if(m_clips2.begin(), m_clips2.end(), [id] (const VideoClip::Holder& clip) {
            return clip->Id() == id;
        });
        if (iter != m_clips2.end())
            return *iter;
        return nullptr;
    }

private:
    recursive_mutex m_apiLock;
    int64_t m_id;
    uint32_t m_outWidth;
    uint32_t m_outHeight;
    Ratio m_frameRate;
    list<VideoClip::Holder> m_clips;
    list<VideoClip::Holder>::iterator m_readClipIter;
    list<VideoClip::Holder> m_clips2;
    bool m_clipChanged{false};
    atomic_bool m_syncReadPos{false};
    recursive_mutex m_clipChangeLock;
    list<VideoOverlap::Holder> m_overlaps;
    list<VideoOverlap::Holder>::iterator m_readOverlapIter;
    mutex m_ovlpUpdateLock;
    int64_t m_readFrames{0};
    int64_t m_duration{0}, m_duration2{0};
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
    if (m_clipChanged)
        UpdateClipState();

    VideoTrack_Impl* newInstance = new VideoTrack_Impl(m_id, outWidth, outHeight, frameRate);
    // duplicate the clips
    for (auto clip : m_clips)
    {
        auto newClip = clip->Clone(outWidth, outHeight, frameRate);
        newClip->SetTrackId(m_id);
        newInstance->m_clips2.push_back(newClip);
        newInstance->m_clipChanged = true;
        newInstance->UpdateClipState();
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
