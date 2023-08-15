#pragma once

#include "MediaCore.h"
#include "MediaInfo.h"
#include "HwaccelManager.h"
#include "immat.h"
#include <memory>
#include <cstdint>

namespace MediaCore
{

struct SharedSettings
{
    using Holder = std::shared_ptr<SharedSettings>;
    static MEDIACORE_API Holder CreateInstance();

    virtual uint32_t VideoOutWidth() const = 0;
    virtual uint32_t VideoOutHeight() const = 0;
    virtual Ratio VideoOutFrameRate() const = 0;
    virtual ImColorFormat VideoOutColorFormat() const = 0;
    virtual ImDataType VideoOutDataType() const = 0;
    virtual HwaccelManager::Holder GetHwaccelManager() const = 0;

    virtual void SetVideoOutWidth(uint32_t width) = 0;
    virtual void SetVideoOutHeight(uint32_t height) = 0;
    virtual void SetVideoOutFrameRate(const Ratio& framerate) = 0;
    virtual void SetVideoOutColorFormat(ImColorFormat colorformat) = 0;
    virtual void SetVideoOutDataType(ImDataType datatype) = 0;
    virtual void SetHwaccelManager(HwaccelManager::Holder hHwaMgr) = 0;
};

}