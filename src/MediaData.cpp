#include "MediaData.h"

namespace MediaCore
{
class VideoFrame_MatImpl : public VideoFrame
{
public:
    VideoFrame_MatImpl(const ImGui::ImMat& vmat) : m_vmat(vmat) {}
    virtual ~VideoFrame_MatImpl() {}

    bool GetMat(ImGui::ImMat& m) override
    {
        m = m_vmat;
        return true;
    }

    int64_t Pos() const override
    {
        return (int64_t)(m_vmat.time_stamp*1000);
    }

    int64_t Pts() const override
    {
        return 0;
    }

    int64_t Dur() const override
    {
        return 0;
    }

    void SetAutoConvertToMat(bool enable) override {}
    bool IsReady() const override { return !m_vmat.empty(); }

    NativeData GetNativeData() const override
    {
        return { NativeData::MAT, (void*)&m_vmat };
    }

private:
    ImGui::ImMat m_vmat;
};

static const auto MEDIA_READER_VIDEO_FRAME_MATIMPL_HOLDER_DELETER = [] (VideoFrame* p) {
    VideoFrame_MatImpl* ptr = dynamic_cast<VideoFrame_MatImpl*>(p);
    delete ptr;
};

VideoFrame::Holder VideoFrame::CreateMatInstance(const ImGui::ImMat& m)
{
    return VideoFrame::Holder(new VideoFrame_MatImpl(m), MEDIA_READER_VIDEO_FRAME_MATIMPL_HOLDER_DELETER);
}
}