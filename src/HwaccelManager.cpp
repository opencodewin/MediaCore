#include <list>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <chrono>
#include <algorithm>
#include "HwaccelManager.h"

extern "C"
{
    #include "libavutil/hwcontext.h"
}

using namespace std;
using namespace Logger;

namespace MediaCore
{

class HwaccelManager_Impl : public HwaccelManager
{
public:
    HwaccelManager_Impl()
    {
        m_logger = GetLogger("HwaMgr");
    }

    bool Init() override
    {
        list<CheckHwaccelThreadContext> checkTaskContexts;
        AVHWDeviceType hwDevType = AV_HWDEVICE_TYPE_NONE;
        do {
            hwDevType = av_hwdevice_iterate_types(hwDevType);
            if (hwDevType != AV_HWDEVICE_TYPE_NONE)
            {
                checkTaskContexts.emplace_back();
                auto& newTask = checkTaskContexts.back();
                newTask.hwDevType = hwDevType;
                newTask.checkThread = thread(&HwaccelManager_Impl::CheckHwaccelUsableProc, this, &newTask);
            }
        } while (hwDevType != AV_HWDEVICE_TYPE_NONE);
        if (!checkTaskContexts.empty())
        {
            while (true)
            {
                this_thread::sleep_for(chrono::milliseconds(10));
                auto iter = find_if(checkTaskContexts.begin(), checkTaskContexts.end(), [] (auto& ctx) {
                    return !ctx.done;
                });
                if (iter == checkTaskContexts.end())
                    break;
            }
            for (auto& ctx : checkTaskContexts)
            {
                m_devices.push_back(ctx.devInfo);
                if (ctx.checkThread.joinable())
                    ctx.checkThread.join();
            }
            checkTaskContexts.clear();
        }

        m_devices.sort([] (auto& a, auto& b) {
            return a.priority < b.priority;
        });
        return true;
    }

    vector<const DeviceInfo*> GetDevices() const override
    {
        vector<const DeviceInfo*> result;
        result.reserve(m_devices.size());
        for (auto& devinfo : m_devices)
            result.push_back(&devinfo);
        return std::move(result);
    }

    void SetLogLevel(Logger::Level l) override
    {
        m_logger->SetShowLevels(l);
    }

    string GetError() const override
    {
        return m_errMsg;
    }

public:
    struct CheckHwaccelThreadContext
    {
        AVHWDeviceType hwDevType;
        DeviceInfo devInfo;
        thread checkThread;
        bool done{false};
    };

    void CheckHwaccelUsableProc(CheckHwaccelThreadContext* ctx)
    {
        int priority;
        switch (ctx->hwDevType)
        {
        case AV_HWDEVICE_TYPE_CUDA:
        case AV_HWDEVICE_TYPE_VIDEOTOOLBOX:
            priority = 0;
            break;
        case AV_HWDEVICE_TYPE_QSV:
        case AV_HWDEVICE_TYPE_VULKAN:
            priority = 1;
            break;
        case AV_HWDEVICE_TYPE_VAAPI:
        case AV_HWDEVICE_TYPE_D3D11VA:
        case AV_HWDEVICE_TYPE_MEDIACODEC:
            priority = 2;
            break;
        case AV_HWDEVICE_TYPE_VDPAU:
        case AV_HWDEVICE_TYPE_DXVA2:
            priority = 3;
            break;
        case AV_HWDEVICE_TYPE_DRM:
        case AV_HWDEVICE_TYPE_OPENCL:
            priority = 9;
            break;
        default:
            priority = 16;
            break;
        }
        string typeName = string(av_hwdevice_get_type_name(ctx->hwDevType));
        bool usable = true;
        AVBufferRef* pDevCtx = nullptr;
        int fferr = av_hwdevice_ctx_create(&pDevCtx, ctx->hwDevType, nullptr, nullptr, 0);
        if (fferr == 0)
        {
            m_logger->Log(DEBUG) << "--> " << typeName << " <-- Check SUCCESSFUL!" << endl;
        }
        else
        {
            usable = false;
            m_logger->Log(DEBUG) << "--> " << typeName << " <-- Check FAILED! fferr=" << fferr << endl;
        }
        if (pDevCtx)
            av_buffer_unref(&pDevCtx);
        ctx->devInfo = {typeName, usable, priority};
        ctx->done = true;
    }

private:
    list<DeviceInfo> m_devices;
    ALogger* m_logger;
    string m_errMsg;
};

static const auto HWACCEL_MANAGER_DELETER = [] (HwaccelManager* pHwaMgr) {
    HwaccelManager_Impl* pImpl = dynamic_cast<HwaccelManager_Impl*>(pHwaMgr);
    delete pImpl;
};

HwaccelManager::Holder HwaccelManager::CreateInstance()
{
    return HwaccelManager::Holder(new HwaccelManager_Impl(), HWACCEL_MANAGER_DELETER);
}

static HwaccelManager::Holder _DEFAULT_HWACCEL_MANAGER;
static mutex _DEFAULT_HWACCEL_MANAGER_ACCESS_LOCK;

HwaccelManager::Holder HwaccelManager::GetDefaultInstance()
{
    lock_guard<mutex> lk(_DEFAULT_HWACCEL_MANAGER_ACCESS_LOCK);
    if (!_DEFAULT_HWACCEL_MANAGER)
        _DEFAULT_HWACCEL_MANAGER = HwaccelManager::CreateInstance();
    return _DEFAULT_HWACCEL_MANAGER;
}
}