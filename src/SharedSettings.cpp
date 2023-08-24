#include <functional>
#include "SharedSettings.h"
#include "FFUtils.h"

using namespace std;

namespace MediaCore
{
class SharedSettings_Impl : public SharedSettings
{
    Holder Clone() override
    {
        Holder newInstance(new SharedSettings_Impl(*this), SHARED_SETTINGS_DELETER);
        return newInstance;
    }

    // getters
    uint32_t VideoOutWidth() const override
    {
        return m_vidOutWidth;
    }

    uint32_t VideoOutHeight() const override
    {
        return m_vidOutHeight;
    }

    Ratio VideoOutFrameRate() const override
    {
        return m_vidOutFrameRate;
    }

    ImColorFormat VideoOutColorFormat() const override
    {
        return m_vidOutColorFormat;
    }

    ImDataType VideoOutDataType() const override
    {
        return m_vidOutDataType;
    }

    HwaccelManager::Holder GetHwaccelManager() const override
    {
        return m_hHwaMgr;
    }

    uint32_t AudioOutChannels() const override
    {
        return m_audOutChannels;
    }

    uint32_t AudioOutSampleRate() const override
    {
        return m_audOutSampleRate;
    }

    ImDataType AudioOutDataType() const override
    {
        return m_audOutDataType;
    }

    bool AudioOutIsPlanar() const override
    {
        return m_audOutIsPlanar;
    }

    string AudioOutSampleFormatName() const override
    {
        return m_audOutSmpfmtName;
    }

    // setters
    void SetVideoOutWidth(uint32_t width) override
    {
        m_vidOutWidth = width;
    }

    void SetVideoOutHeight(uint32_t height) override
    {
        m_vidOutHeight = height;
    }

    void SetVideoOutFrameRate(const Ratio& framerate) override
    {
        m_vidOutFrameRate = framerate;
    }

    void SetVideoOutColorFormat(ImColorFormat colorformat) override
    {
        m_vidOutColorFormat = colorformat;
    }

    void SetVideoOutDataType(ImDataType datatype) override
    {
        m_vidOutDataType = datatype;
    }

    void SetHwaccelManager(HwaccelManager::Holder hHwaMgr) override
    {
        m_hHwaMgr = hHwaMgr;
    }

    void SetAudioOutChannels(uint32_t channels) override
    {
        m_audOutChannels = channels;
    }

    void SetAudioOutSampleRate(uint32_t sampleRate) override
    {
        m_audOutSampleRate = sampleRate;
    }

    void SetAudioOutDataType(ImDataType dataType) override
    {
        m_audOutDataType = dataType;
        m_audOutSmpfmt = GetAVSampleFormatByDataType(dataType, m_audOutIsPlanar);
        auto pcSmpfmtName = av_get_sample_fmt_name(m_audOutSmpfmt);
        m_audOutSmpfmtName = pcSmpfmtName ? string(pcSmpfmtName) : "None";
    }

    void SetAudioOutIsPlanar(bool isPlanar) override
    {
        m_audOutIsPlanar = isPlanar;
        m_audOutSmpfmt = GetAVSampleFormatByDataType(m_audOutDataType, isPlanar);
        auto pcSmpfmtName = av_get_sample_fmt_name(m_audOutSmpfmt);
        m_audOutSmpfmtName = pcSmpfmtName ? string(pcSmpfmtName) : "None";
    }

    void SyncAudioSettingsFrom(SharedSettings* pSettings) override
    {
        SetAudioOutChannels(pSettings->AudioOutChannels());
        SetAudioOutSampleRate(pSettings->AudioOutSampleRate());
        SetAudioOutDataType(pSettings->AudioOutDataType());
        SetAudioOutIsPlanar(pSettings->AudioOutIsPlanar());
    }

public:
    static const function<void(SharedSettings*)> SHARED_SETTINGS_DELETER;

private:
    uint32_t m_vidOutWidth{0};
    uint32_t m_vidOutHeight{0};
    Ratio m_vidOutFrameRate;
    ImColorFormat m_vidOutColorFormat{IM_CF_RGBA};
    ImDataType m_vidOutDataType{IM_DT_FLOAT32};
    HwaccelManager::Holder m_hHwaMgr;
    uint32_t m_audOutChannels{0};
    uint32_t m_audOutSampleRate{0};
    ImDataType m_audOutDataType{IM_DT_FLOAT32};
    bool m_audOutIsPlanar{false};
    AVSampleFormat m_audOutSmpfmt{AV_SAMPLE_FMT_NONE};
    string m_audOutSmpfmtName{"None"};
};

const function<void(SharedSettings*)> SharedSettings_Impl::SHARED_SETTINGS_DELETER = [] (SharedSettings* p) {
    SharedSettings_Impl* ptr = dynamic_cast<SharedSettings_Impl*>(p);
    delete ptr;
};

SharedSettings::Holder SharedSettings::CreateInstance()
{
    return SharedSettings::Holder(new SharedSettings_Impl(), SharedSettings_Impl::SHARED_SETTINGS_DELETER);
}
}