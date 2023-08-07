#pragma once
#include <cstdint>
#include "MediaCore.h"
#include "immat.h"

namespace MatUtils
{
    MEDIACORE_API void CopyAudioMatSamples(ImGui::ImMat& dstMat, const ImGui::ImMat& srcMat, uint32_t dstOffSmpCnt, uint32_t srcOffSmpCnt, uint32_t copySmpCnt = 0);
}