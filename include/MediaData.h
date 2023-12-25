#pragma once
#include <cstdint>
#include <memory>
#include <immat.h>
#include "MediaCore.h"

namespace MediaCore
{
    struct VideoFrame
    {
        using Holder = std::shared_ptr<VideoFrame>;
        static MEDIACORE_API Holder CreateMatInstance(const ImGui::ImMat& m);

        virtual bool GetMat(ImGui::ImMat& m) = 0;
        virtual int64_t Pos() const = 0;
        virtual int64_t Pts() const = 0;
        virtual int64_t Dur() const = 0;
        virtual void SetAutoConvertToMat(bool enable) = 0;
        virtual bool IsReady() const = 0;

        struct NativeData
        {
            enum Type
            {
                UNKNOWN = 0,
                AVFRAME,
                AVFRAME_HOLDER,
                MAT,
            };
            enum Type eType;
            void* pData;
        };
        virtual NativeData GetNativeData() const = 0;
    };
}