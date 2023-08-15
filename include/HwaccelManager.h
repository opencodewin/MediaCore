#pragma once
#include <vector>
#include <string>
#include <memory>
#include "MediaCore.h"
#include "Logger.h"

namespace MediaCore
{
struct HwaccelManager
{
    using Holder = std::shared_ptr<HwaccelManager>;
    static MEDIACORE_API Holder CreateInstance();
    static MEDIACORE_API Holder GetDefaultInstance();

    virtual bool Init() = 0;

    struct DeviceInfo
    {
        std::string deviceType;
        bool usable;
        int priority;
    };
    virtual std::vector<const DeviceInfo*> GetDevices() const = 0;

    virtual void SetLogLevel(Logger::Level l) = 0;
    virtual std::string GetError() const = 0;
};
}