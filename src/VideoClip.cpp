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

#include <imconfig.h>
#if IMGUI_VULKAN_SHADER
#include <ColorConvert_vulkan.h>
#include <AlphaBlending_vulkan.h>
#endif
#include "VideoClip.h"
#include "VideoTransformFilter.h"
#include "Logger.h"
#include "DebugHelper.h"
#include "SysUtils.h"

using namespace std;
using namespace Logger;

namespace MediaCore
{
bool VideoClip::USE_HWACCEL = true;

///////////////////////////////////////////////////////////////////////////////////////////
// VideoClip_VideoImpl
///////////////////////////////////////////////////////////////////////////////////////////
class VideoClip_VideoImpl : public VideoClip
{
public:
    VideoClip_VideoImpl(
        int64_t id, MediaParser::Holder hParser,
        uint32_t outWidth, uint32_t outHeight, const Ratio& frameRate,
        int64_t start, int64_t startOffset, int64_t endOffset, int64_t readpos, bool forward)
        : m_id(id), m_start(start)
    {
        string fileName = SysUtils::ExtractFileName(hParser->GetUrl());
        ostringstream loggerNameOss;
        loggerNameOss << id;
        string idstr = loggerNameOss.str();
        if (idstr.size() > 4)
            idstr = idstr.substr(idstr.size()-4);
        loggerNameOss.str(""); loggerNameOss << "VClp-" << fileName.substr(0, 4) << "-" << idstr;

        m_logger = GetLogger(loggerNameOss.str());
        m_hInfo = hParser->GetMediaInfo();
        if (hParser->GetBestVideoStreamIndex() < 0)
            throw invalid_argument("Argument 'hParser' has NO VIDEO stream!");
        auto vidStm = hParser->GetBestVideoStream();
        if (vidStm->isImage)
            throw invalid_argument("This video stream is an IMAGE, it should be instantiated with a 'VideoClip_ImageImpl' instance!");
        loggerNameOss.str(""); loggerNameOss << "VRdr-" << fileName.substr(0, 4) << "-" << idstr;
        m_hReader = MediaReader::CreateVideoInstance(loggerNameOss.str());
        m_hReader->EnableHwAccel(VideoClip::USE_HWACCEL);
        if (!m_hReader->Open(hParser))
            throw runtime_error(m_hReader->GetError());
        uint32_t readerWidth, readerHeight;
        if (outWidth*vidStm->height > outHeight*vidStm->width)
        {
            readerHeight = outHeight;
            readerWidth = vidStm->width*outHeight/vidStm->height;
        }
        else
        {
            readerWidth = outWidth;
            readerHeight = vidStm->height*outWidth/vidStm->width;
        }
        readerWidth += readerWidth&0x1;
        readerHeight += readerHeight&0x1;
        ImInterpolateMode interpMode = IM_INTERPOLATE_BICUBIC;
        if (readerWidth*readerHeight < vidStm->width*vidStm->height)
            interpMode = IM_INTERPOLATE_AREA;
        if (!m_hReader->ConfigVideoReader(readerWidth, readerHeight, IM_CF_RGBA, interpMode))
            throw runtime_error(m_hReader->GetError());
        if (frameRate.num <= 0 || frameRate.den <= 0)
            throw invalid_argument("Invalid argument value for 'frameRate'!");
        m_frameRate = frameRate;
        m_srcDuration = static_cast<int64_t>(m_hReader->GetVideoStream()->duration*1000);
        if (startOffset < 0)
            throw invalid_argument("Argument 'startOffset' can NOT be NEGATIVE!");
        if (endOffset < 0)
            throw invalid_argument("Argument 'endOffset' can NOT be NEGATIVE!");
        if (startOffset+endOffset >= m_srcDuration*1000)
            throw invalid_argument("Argument 'startOffset/endOffset', clip duration is NOT LARGER than 0!");
        m_startOffset = startOffset;
        m_endOffset = endOffset;
        m_hReader->SetDirection(forward);
        if (!m_hReader->SeekTo((double)startOffset/1000))
            throw runtime_error(m_hReader->GetError());
        bool suspend = readpos < -m_wakeupRange || readpos > Duration()+m_wakeupRange;
        if (!m_hReader->Start(suspend))
            throw runtime_error(m_hReader->GetError());
        m_hWarpFilter = CreateVideoTransformFilter();
        if (!m_hWarpFilter->Initialize(outWidth, outHeight))
            throw runtime_error(m_hWarpFilter->GetError());
    }

    ~VideoClip_VideoImpl()
    {
    }

    Holder Clone(uint32_t outWidth, uint32_t outHeight, const Ratio& frameRate) const override;

    MediaParser::Holder GetMediaParser() const override
    {
        return m_hReader->GetMediaParser();
    }

    int64_t Id() const override
    {
        return m_id;
    }
    
    int64_t TrackId() const override
    {
        return m_trackId;
    }

    bool IsImage() const override
    {
        return false;
    }

    int64_t Start() const override
    {
        return m_start;
    }

    int64_t End() const override
    {
        return m_start+Duration();
    }

    int64_t StartOffset() const override
    {
        return m_startOffset;
    }

    int64_t EndOffset() const override
    {
        return m_endOffset;
    }

    int64_t Duration() const override
    {
        return m_srcDuration-m_startOffset-m_endOffset;
    }

    uint32_t SrcWidth() const override
    {
        return m_hReader->GetVideoOutWidth();
    }

    uint32_t SrcHeight() const override
    {
        return m_hReader->GetVideoOutHeight();
    }

    uint32_t OutWidth() const override
    {
        return m_hWarpFilter->GetOutWidth();
    }

    uint32_t OutHeight() const override
    {
        return m_hWarpFilter->GetOutHeight();
    }

    void SetTrackId(int64_t trackId) override
    {
        m_trackId = trackId;
    }

    void SetStart(int64_t start) override
    {
        m_start = start;
    }

    void ChangeStartOffset(int64_t startOffset) override
    {
        if (startOffset == m_startOffset)
            return;
        if (startOffset < 0)
            throw invalid_argument("Argument 'startOffset' can NOT be NEGATIVE!");
        if (startOffset+m_endOffset >= m_srcDuration)
            throw invalid_argument("Argument 'startOffset/endOffset', clip duration is NOT LARGER than 0!");
        m_startOffset = startOffset;
    }

    void ChangeEndOffset(int64_t endOffset) override
    {
        if (endOffset == m_endOffset)
            return;
        if (endOffset < 0)
            throw invalid_argument("Argument 'endOffset' can NOT be NEGATIVE!");
        if (m_startOffset+endOffset >= m_srcDuration)
            throw invalid_argument("Argument 'startOffset/endOffset', clip duration is NOT LARGER than 0!");
        m_endOffset = endOffset;
    }

    void SetDuration(int64_t duration) override
    {
        throw runtime_error("'VideoClip_VideoImpl' dose NOT SUPPORT setting duration!");
    }

    void ReadVideoFrame(int64_t pos, vector<CorrelativeFrame>& frames, ImGui::ImMat& out, bool& eof) override
    {
        if (m_eof)
        {
            eof = true;
            return;
        }
        if (m_hReader->IsSuspended())
        {
            m_hReader->Wakeup();
            // Log(DEBUG) << ">>>> Clip#" << m_id <<" is WAKEUP." << endl;
        }

        ImGui::ImMat image;
        // string filename = SysUtils::ExtractFileBaseName(m_hInfo->url);
        // AddCheckPoint(filename+", t0");
        const double readPosTs = (double)(pos+m_startOffset)/1000;
        if (!m_hReader->ReadVideoFrame(readPosTs, image, eof))
        {
            m_logger->Log(WARN) << "FAILED to read frame @ timeline-pos=" << pos << "ms, media-time=" << readPosTs << "s! Error is '" << m_hReader->GetError() << "'." << endl;
            return;
        }
        // AddCheckPoint(filename+", t1");
        // LogCheckPointsTimeInfo();
        frames.push_back({CorrelativeFrame::PHASE_SOURCE_FRAME, m_id, m_trackId, image});

        // process with external filter
        auto hFilter = m_hFilter;
        if (hFilter)
            image = hFilter->FilterImage(image, pos/*+m_start*/);
        frames.push_back({CorrelativeFrame::PHASE_AFTER_FILTER, m_id, m_trackId, image});

        // process with transform filter
        image = m_hWarpFilter->FilterImage(image, pos/*+m_start*/);
        frames.push_back({CorrelativeFrame::PHASE_AFTER_TRANSFORM, m_id, m_trackId, image});
        out = image;
    }

    void SeekTo(int64_t pos) override
    {
        if (pos > Duration())
            return;
        if (pos < 0)
            pos = 0;
        if (!m_hReader->SeekTo((double)(pos+m_startOffset)/1000))
            throw runtime_error(m_hReader->GetError());
        m_eof = false;
    }

    void NotifyReadPos(int64_t pos) override
    {
        if (pos < -m_wakeupRange || pos > Duration()+m_wakeupRange)
        {
            if (!m_hReader->IsSuspended())
            {
                m_hReader->Suspend();
                // Log(DEBUG) << ">>>> Clip#" << m_id <<" is SUSPENDED." << endl;
            }
        }
        else if (m_hReader->IsSuspended())
        {
            m_hReader->Wakeup();
            // Log(DEBUG) << ">>>> Clip#" << m_id <<" is WAKEUP." << endl;
        }
    }

    void SetDirection(bool forward) override
    {
        m_hReader->SetDirection(forward);
    }

    void SetFilter(VideoFilter::Holder filter) override
    {
        if (filter)
        {
            filter->ApplyTo(this);
            m_hFilter = filter;
        }
        else
        {
            m_hFilter = nullptr;
        }
    }

    VideoFilter::Holder GetFilter() const override
    {
        return m_hFilter;
    }

    VideoTransformFilterHolder GetTransformFilter() override
    {
        return m_hWarpFilter;
    }

private:
    ALogger* m_logger;
    int64_t m_id;
    int64_t m_trackId{-1};
    MediaInfo::Holder m_hInfo;
    MediaReader::Holder m_hReader;
    int64_t m_srcDuration;
    int64_t m_start;
    int64_t m_startOffset;
    int64_t m_endOffset;
    bool m_eof{false};
    Ratio m_frameRate;
    uint32_t m_frameIndex{0};
    VideoFilter::Holder m_hFilter;
    VideoTransformFilterHolder m_hWarpFilter;
    int64_t m_wakeupRange{1000};
};

static const auto VIDEO_CLIP_HOLDER_VIDEOIMPL_DELETER = [] (VideoClip* p) {
    VideoClip_VideoImpl* ptr = dynamic_cast<VideoClip_VideoImpl*>(p);
    delete ptr;
};

VideoClip::Holder VideoClip::CreateVideoInstance(
        int64_t id, MediaParser::Holder hParser,
        uint32_t outWidth, uint32_t outHeight, const Ratio& frameRate,
        int64_t start, int64_t startOffset, int64_t endOffset, int64_t readpos, bool forward)
{
    return VideoClip::Holder(new VideoClip_VideoImpl(id, hParser, outWidth, outHeight, frameRate, start, startOffset, endOffset, readpos, true),
            VIDEO_CLIP_HOLDER_VIDEOIMPL_DELETER);
}

VideoClip::Holder VideoClip_VideoImpl::Clone(uint32_t outWidth, uint32_t outHeight, const Ratio& frameRate) const
{
    VideoClip_VideoImpl* newInstance = new VideoClip_VideoImpl(
        m_id, m_hReader->GetMediaParser(), outWidth, outHeight, frameRate, m_start, m_startOffset, m_endOffset, 0, true);
    if (m_hFilter) newInstance->SetFilter(m_hFilter->Clone());
    newInstance->m_hWarpFilter = m_hWarpFilter->Clone(outWidth, outHeight);
    return VideoClip::Holder(newInstance, VIDEO_CLIP_HOLDER_VIDEOIMPL_DELETER);
}

class VideoClip_ImageImpl : public VideoClip
{
public:
    VideoClip_ImageImpl(
        int64_t id, MediaParser::Holder hParser,
        uint32_t outWidth, uint32_t outHeight, int64_t start, int64_t duration)
        : m_id(id), m_start(start)
    {
        m_hInfo = hParser->GetMediaInfo();
        if (hParser->GetBestVideoStreamIndex() < 0)
            throw invalid_argument("Argument 'hParser' has NO VIDEO stream!");
        auto vidStm = hParser->GetBestVideoStream();
        if (!vidStm->isImage)
            throw invalid_argument("This video stream is NOT an IMAGE, it should be instantiated with a 'VideoClip_VideoImpl' instance!");
        m_hReader = MediaReader::CreateInstance();
        if (!m_hReader->Open(hParser))
            throw runtime_error(m_hReader->GetError());
        uint32_t readerWidth, readerHeight;
        if (outWidth*vidStm->height > outHeight*vidStm->width)
        {
            readerHeight = outHeight;
            readerWidth = vidStm->width*outHeight/vidStm->height;
        }
        else
        {
            readerWidth = outWidth;
            readerHeight = vidStm->height*outWidth/vidStm->width;
        }
        readerWidth += readerWidth&0x1;
        readerHeight += readerHeight&0x1;
        ImInterpolateMode interpMode = IM_INTERPOLATE_BICUBIC;
        if (readerWidth*readerHeight < vidStm->width*vidStm->height)
            interpMode = IM_INTERPOLATE_AREA;
        if (!m_hReader->ConfigVideoReader(readerWidth, readerHeight, IM_CF_RGBA, interpMode))
            throw runtime_error(m_hReader->GetError());
        if (duration <= 0)
            throw invalid_argument("Argument 'duration' must be positive!");
        m_srcDuration = duration;
        m_start = start;
        if (!m_hReader->Start())
            throw runtime_error(m_hReader->GetError());
        m_hWarpFilter = CreateVideoTransformFilter();
        if (!m_hWarpFilter->Initialize(outWidth, outHeight))
            throw runtime_error(m_hWarpFilter->GetError());
    }

    ~VideoClip_ImageImpl()
    {
    }

    Holder Clone(uint32_t outWidth, uint32_t outHeight, const Ratio& frameRate) const override;

    MediaParser::Holder GetMediaParser() const override
    {
        return m_hReader->GetMediaParser();
    }

    int64_t Id() const override
    {
        return m_id;
    }
    
    int64_t TrackId() const override
    {
        return m_trackId;
    }

    bool IsImage() const override
    {
        return true;
    }

    int64_t Start() const override
    {
        return m_start;
    }

    int64_t End() const override
    {
        return m_start+Duration();
    }

    int64_t StartOffset() const override
    {
        return 0;
    }

    int64_t EndOffset() const override
    {
        return 0;
    }

    int64_t Duration() const override
    {
        return m_srcDuration;
    }

    uint32_t SrcWidth() const override
    {
        return m_hReader->GetVideoOutWidth();
    }

    uint32_t SrcHeight() const override
    {
        return m_hReader->GetVideoOutHeight();
    }

    uint32_t OutWidth() const override
    {
        return m_hWarpFilter->GetOutWidth();
    }

    uint32_t OutHeight() const override
    {
        return m_hWarpFilter->GetOutHeight();
    }

    void SetTrackId(int64_t trackId) override
    {
        m_trackId = trackId;
    }

    void SetStart(int64_t start) override
    {
        m_start = start;
    }

    void ChangeStartOffset(int64_t startOffset) override
    {}

    void ChangeEndOffset(int64_t endOffset) override
    {}

    void SetDuration(int64_t duration) override
    {
        if (duration <= 0)
            throw invalid_argument("Argument 'duration' must be a positive integer!");
        m_srcDuration = duration;
    }

    void ReadVideoFrame(int64_t pos, vector<CorrelativeFrame>& frames, ImGui::ImMat& out, bool& eof) override
    {
        ImGui::ImMat image;
        if (!m_hReader->ReadVideoFrame(0, image, eof))
            throw runtime_error(m_hReader->GetError());
        frames.push_back({CorrelativeFrame::PHASE_SOURCE_FRAME, m_id, m_trackId, image});

        // process with external filter
        VideoFilter::Holder filter = m_hFilter;
        if (filter)
            image = filter->FilterImage(image, pos/*+m_start*/);
        frames.push_back({CorrelativeFrame::PHASE_AFTER_FILTER, m_id, m_trackId, image});

        // process with transform filter
        image = m_hWarpFilter->FilterImage(image, pos/*+m_start*/);
        frames.push_back({CorrelativeFrame::PHASE_AFTER_TRANSFORM, m_id, m_trackId, image});
        out = image;
    }

    void SeekTo(int64_t pos) override
    {}

    void NotifyReadPos(int64_t pos) override
    {}

    void SetDirection(bool forward) override
    {}

    void SetFilter(VideoFilter::Holder filter) override
    {
        if (filter)
        {
            filter->ApplyTo(this);
            m_hFilter = filter;
        }
        else
        {
            m_hFilter = nullptr;
        }
    }

    VideoFilter::Holder GetFilter() const override
    {
        return m_hFilter;
    }

    VideoTransformFilterHolder GetTransformFilter() override
    {
        return m_hWarpFilter;
    }

private:
    int64_t m_id;
    int64_t m_trackId{-1};
    MediaInfo::Holder m_hInfo;
    MediaReader::Holder m_hReader;
    int64_t m_srcDuration;
    int64_t m_start;
    VideoFilter::Holder m_hFilter;
    VideoTransformFilterHolder m_hWarpFilter;
};

static const auto VIDEO_CLIP_HOLDER_IMAGEIMPL_DELETER = [] (VideoClip* p) {
    VideoClip_ImageImpl* ptr = dynamic_cast<VideoClip_ImageImpl*>(p);
    delete ptr;
};

VideoClip::Holder VideoClip::CreateImageInstance(
        int64_t id, MediaParser::Holder hParser,
        uint32_t outWidth, uint32_t outHeight, int64_t start, int64_t duration)
{
    return VideoClip::Holder(new VideoClip_ImageImpl(id, hParser, outWidth, outHeight, start, duration),
            VIDEO_CLIP_HOLDER_IMAGEIMPL_DELETER);
}

VideoClip::Holder VideoClip_ImageImpl::Clone(uint32_t outWidth, uint32_t outHeight, const Ratio& frameRate) const
{
    VideoClip_ImageImpl* newInstance = new VideoClip_ImageImpl(
        m_id, m_hReader->GetMediaParser(), outWidth, outHeight, m_start, m_srcDuration);
    if (m_hFilter) newInstance->SetFilter(m_hFilter->Clone());
    newInstance->m_hWarpFilter = m_hWarpFilter->Clone(outWidth, outHeight);
    return VideoClip::Holder(newInstance, VIDEO_CLIP_HOLDER_IMAGEIMPL_DELETER);
}

ostream& operator<<(ostream& os, VideoClip::Holder hClip)
{
    if (hClip->IsImage())
        os << "(I){'id':" << hClip->Id() << ", 'start':" << hClip->Start() << ", 'dur':" << hClip->Duration() << "}";
    else
        os << "(V){'id':" << hClip->Id() << ", 'start':" << hClip->Start() << ", 'dur':" << hClip->Duration()
            << ", 'soff':" << hClip->StartOffset() << ", 'eoff':" << hClip->EndOffset() << "}";
    return os;
}


///////////////////////////////////////////////////////////////////////////////////////////
// DefaultVideoTransition_Impl
///////////////////////////////////////////////////////////////////////////////////////////
class DefaultVideoTransition_Impl : public VideoTransition
{
public:
    Holder Clone() override
    {
        return Holder(new DefaultVideoTransition_Impl);
    }

    void ApplyTo(VideoOverlap* overlap) override
    {
        m_overlapPtr = overlap;
    }

    ImGui::ImMat MixTwoImages(const ImGui::ImMat& vmat1, const ImGui::ImMat& vmat2, int64_t pos, int64_t dur) override
    {
#if IMGUI_VULKAN_SHADER
        ImGui::ImMat dst;
        dst.type = IM_DT_INT8;
        double alpha = 1-(double)pos/m_overlapPtr->Duration();
        m_alphaBlender.blend(vmat1, vmat2, dst, (float)alpha);
        return dst;
#else
        return pos < m_overlapPtr->Duration()/2 ? vmat1 : vmat2;
#endif
    }

private:
    VideoOverlap* m_overlapPtr{nullptr};
#if IMGUI_VULKAN_SHADER
    ImGui::ColorConvert_vulkan m_clrcvt;
    ImGui::AlphaBlending_vulkan m_alphaBlender;
#endif
};

///////////////////////////////////////////////////////////////////////////////////////////
// VideoOverlap
///////////////////////////////////////////////////////////////////////////////////////////
class VideoOverlap_Impl : public VideoOverlap
{
public:
    VideoOverlap_Impl(int64_t id, VideoClip::Holder hClip1, VideoClip::Holder hClip2)
        : m_id(id), m_hFrontClip(hClip1), m_hRearClip(hClip2), m_hTrans(new DefaultVideoTransition_Impl())
    {
        ostringstream loggerNameOss;
        loggerNameOss << "VOvlp#" << id;
        m_logger = GetLogger(loggerNameOss.str());
        Update();
        m_hTrans->ApplyTo(this);
    }

    int64_t Id() const override
    {
        return m_id;
    }

    void SetId(int64_t id) override
    {
        m_id = id;
    }

    int64_t Start() const override
    {
        return m_start;
    }

    int64_t End() const override
    {
        return m_end;
    }

    int64_t Duration() const override
    {
        return m_end-m_start;
    }

    VideoClip::Holder FrontClip() const override
    {
        return m_hFrontClip;
    }

    VideoClip::Holder RearClip() const override
    {
        return m_hRearClip;
    }

    void ReadVideoFrame(int64_t pos, vector<CorrelativeFrame>& frames, ImGui::ImMat& out, bool& eof) override
    {
        if (pos < 0 || pos > Duration())
            throw invalid_argument("Argument 'pos' can NOT be NEGATIVE or larger than overlap duration!");

        ImGui::ImMat vmat1;
        bool eof1{false};
        int64_t pos1 = pos+(Start()-m_hFrontClip->Start());
        m_hFrontClip->ReadVideoFrame(pos1, frames, vmat1, eof1);

        ImGui::ImMat vmat2;
        bool eof2{false};
        int64_t pos2 = pos+(Start()-m_hRearClip->Start());
        m_hRearClip->ReadVideoFrame(pos2, frames, vmat2, eof2);

        eof = eof1 || eof2;
        if (pos == Duration())
            eof = true;

        if (vmat1.empty())
        {
            m_logger->Log(WARN) << "'vmat1' is EMPTY!" << endl;
            out = vmat2;
            return;
        }
        if (vmat2.empty())
        {
            m_logger->Log(WARN) << "'vmat2' is EMPTY!" << endl;
            out = vmat1;
            return;
        }

        auto hTrans = m_hTrans;
        out = hTrans->MixTwoImages(vmat1, vmat2, pos+m_start, Duration());
        frames.push_back({CorrelativeFrame::PHASE_AFTER_TRANSITION, m_hFrontClip->Id(), m_hFrontClip->TrackId(), out});
    }

    void SeekTo(int64_t pos) override
    {
        if (pos > Duration())
            return;
        if (pos < 0)
            pos = 0;
        int64_t pos1 = pos+(Start()-m_hFrontClip->Start());
        m_hFrontClip->SeekTo(pos1);
        int64_t pos2 = pos+(Start()-m_hRearClip->Start());
        m_hRearClip->SeekTo(pos2);
    }

    void Update() override
    {
        auto hClip1 = m_hFrontClip;
        auto hClip2 = m_hRearClip;
        if (hClip1->Start() <= hClip2->Start())
        {
            m_hFrontClip = hClip1;
            m_hRearClip = hClip2;
        }
        else
        {
            m_hFrontClip = hClip2;
            m_hRearClip = hClip1;
        }
        if (m_hFrontClip->End() <= m_hRearClip->Start())
        {
            m_start = m_end = 0;
        }
        else
        {
            m_start = m_hRearClip->Start();
            m_end = m_hFrontClip->End() <= m_hRearClip->End() ? m_hFrontClip->End() : m_hRearClip->End();
        }
    }

    VideoTransition::Holder GetTransition() const override
    {
        return m_hTrans;
    }

    void SetTransition(VideoTransition::Holder hTrans) override
    {
        if (hTrans)
        {
            hTrans->ApplyTo(this);
            m_hTrans = hTrans;
        }
        else
        {
            VideoTransition::Holder defaultTrans(new DefaultVideoTransition_Impl());
            defaultTrans->ApplyTo(this);
            m_hTrans = defaultTrans;
        }
    }

private:
    ALogger* m_logger;
    int64_t m_id;
    VideoClip::Holder m_hFrontClip;
    VideoClip::Holder m_hRearClip;
    int64_t m_start{0};
    int64_t m_end{0};
    VideoTransition::Holder m_hTrans;
};

bool VideoOverlap::HasOverlap(VideoClip::Holder hClip1, VideoClip::Holder hClip2)
{
    return (hClip1->Start() >= hClip2->Start() && hClip1->Start() < hClip2->End()) ||
            (hClip1->End() > hClip2->Start() && hClip1->End() <= hClip2->End()) ||
            (hClip1->Start() < hClip2->Start() && hClip1->End() > hClip2->End());
}

VideoOverlap::Holder VideoOverlap::CreateInstance(int64_t id, VideoClip::Holder hClip1, VideoClip::Holder hClip2)
{
    return VideoOverlap::Holder(new VideoOverlap_Impl(id, hClip1, hClip2), [] (VideoOverlap* p) {
        VideoOverlap_Impl* ptr = dynamic_cast<VideoOverlap_Impl*>(p);
        delete ptr;
    });
}

std::ostream& operator<<(std::ostream& os, VideoOverlap::Holder hOverlap)
{
    os << "{'id':" << hOverlap->Id() << ", 'start':" << hOverlap->Start() << ", 'dur':" << hOverlap->Duration() << "}";
    return os;
}
}
