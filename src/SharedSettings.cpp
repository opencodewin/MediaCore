#include "SharedSettings.h"

using namespace std;

namespace MediaCore
{
class SharedSettings_Impl : public SharedSettings
{
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

private:
    uint32_t m_vidOutWidth{0};
    uint32_t m_vidOutHeight{0};
    Ratio m_vidOutFrameRate;
    ImColorFormat m_vidOutColorFormat{IM_CF_RGBA};
    ImDataType m_vidOutDataType{IM_DT_FLOAT32};
    HwaccelManager::Holder m_hHwaMgr;
};

static const auto SHARED_SETTINGS_DELETER = [] (SharedSettings* p) {
    SharedSettings_Impl* ptr = dynamic_cast<SharedSettings_Impl*>(p);
    delete ptr;
};

SharedSettings::Holder SharedSettings::CreateInstance()
{
    return SharedSettings::Holder(new SharedSettings_Impl(), SHARED_SETTINGS_DELETER);
}
}