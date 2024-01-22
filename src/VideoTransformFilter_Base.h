/*
    Copyright (c) 2023-2024 CodeWin

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

#pragma once
#include <MatUtilsImVecHelper.h>
#include "VideoTransformFilter.h"

using namespace std;
namespace LibCurve = ImGui::ImNewCurve;

namespace MediaCore
{
class VideoTransformFilter_Base : public VideoTransformFilter
{
public:
    VideoTransformFilter_Base()
    {
        m_hPosOffsetCurve = LibCurve::Curve::CreateInstance("PosOffsetCurve", LibCurve::Linear, {-1,-1,0,0}, {1,1,0,0}, {0,0,0,0}, true);
        m_aCropCurves.resize(2);
        m_aCropCurves[0] = LibCurve::Curve::CreateInstance("CropCurveLT", LibCurve::Linear, {0,0,0,0}, {1,1,0,0}, {0,0,0,0}, true);
        m_aCropCurves[1] = LibCurve::Curve::CreateInstance("CropCurveRB", LibCurve::Linear, {0,0,0,0}, {1,1,0,0}, {0,0,0,0}, true);
        m_hScaleCurve = LibCurve::Curve::CreateInstance("ScaleCurve", LibCurve::Linear, {0,0,0,0}, {32,32,0,0}, {1,1,0,0}, true);
        m_hRotationCurve = LibCurve::Curve::CreateInstance("RotationCurve", LibCurve::Linear, {-360,-360,0,0}, {360,360,0,0}, {0,0,0,0}, true);
        m_hOpacityCurve = LibCurve::Curve::CreateInstance("OpacityCurve", LibCurve::Linear, {0,0,0,0}, {1,1,0,0}, {1,1,0,0}, true);
    }

    virtual ~VideoTransformFilter_Base() {}

    Holder Clone(SharedSettings::Holder hSettings) override
    {
        VideoTransformFilter::Holder hNewInst = VideoTransformFilter::CreateInstance();
        if (!hNewInst->Initialize(hSettings))
            return nullptr;
        VideoTransformFilter_Base* pBaseFilter = dynamic_cast<VideoTransformFilter_Base*>(hNewInst.get());
        pBaseFilter->m_tTimeRange = m_tTimeRange;
        pBaseFilter->m_eAspectFitType = m_eAspectFitType;
        pBaseFilter->m_hPosOffsetCurve = m_hPosOffsetCurve->Clone();
        pBaseFilter->m_bEnableKeyFramesOnPosOffset = m_bEnableKeyFramesOnPosOffset;
        pBaseFilter->m_aCropCurves[0] = m_aCropCurves[0]->Clone();
        pBaseFilter->m_aCropCurves[1] = m_aCropCurves[1]->Clone();
        pBaseFilter->m_bEnableKeyFramesOnCrop = m_bEnableKeyFramesOnCrop;
        pBaseFilter->m_bKeepAspectRatio = m_bKeepAspectRatio;
        pBaseFilter->m_hScaleCurve = m_hScaleCurve->Clone();
        pBaseFilter->m_bEnableKeyFramesOnScale = m_bEnableKeyFramesOnScale;
        pBaseFilter->m_hRotationCurve = m_hRotationCurve->Clone();
        pBaseFilter->m_bEnableKeyFramesOnRotation = m_bEnableKeyFramesOnRotation;
        pBaseFilter->m_hOpacityCurve = m_hOpacityCurve->Clone();
        pBaseFilter->m_bEnableKeyFramesOnOpacity = m_bEnableKeyFramesOnOpacity;
        return hNewInst;
    }

    uint32_t GetInWidth() const override
    { return m_u32InWidth; }

    uint32_t GetInHeight() const override
    { return m_u32InHeight; }

    uint32_t GetOutWidth() const override
    { return m_u32OutWidth; }

    uint32_t GetOutHeight() const override
    { return m_u32OutHeight; }

    string GetOutputFormat() const override
    { return m_strOutputFormat; }

    bool SetAspectFitType(AspectFitType type) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (type < ASPECT_FIT_TYPE__FIT || type > ASPECT_FIT_TYPE__STRETCH)
        {
            m_strErrMsg = "INVALID argument 'type'!";
            return false;
        }
        if (m_eAspectFitType == type)
            return true;
        m_eAspectFitType = type;
        m_bNeedUpdateScaleParam = true;
        return true;
    }

    AspectFitType GetAspectFitType() const override
    { return m_eAspectFitType; }

    bool SetTimeRange(const MatUtils::Vec2<int64_t>& tTimeRange) override
    {
        const auto v2TimeRange = MatUtils::ToImVec2(tTimeRange);
        m_hPosOffsetCurve->SetTimeRange(v2TimeRange, true);
        m_tTimeRange = tTimeRange;
        return true;
    }

    MatUtils::Vec2<int64_t> GetTimeRange() const override
    {
        return m_tTimeRange;
    }

    virtual ImGui::ImMat FilterImage(const ImGui::ImMat& vmat, int64_t pos) override = 0;

    VideoFrame::Holder FilterImage(VideoFrame::Holder hVfrm, int64_t pos) override
    {
        if (!hVfrm)
        {
            m_strErrMsg = "INVALID arguments! 'hVfrm' is null.";
            return nullptr;
        }
        ImGui::ImMat vmat;
        const auto bRet = hVfrm->GetMat(vmat);
        if (!bRet || vmat.empty())
        {
            m_strErrMsg = "FAILED to get ImMat instance from 'hVfrm'!";
            return nullptr;
        }
        vmat = this->FilterImage(vmat, pos);
        if (vmat.empty())
            return nullptr;
        return VideoFrame::CreateMatInstance(vmat);
    }

    // Position
    bool SetPosOffset(int32_t i32PosOffX, int32_t i32PosOffY) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (m_i32PosOffsetX == i32PosOffX && m_i32PosOffsetY == i32PosOffY)
            return true;
        if (m_u32OutWidth == 0 || m_u32OutHeight == 0)
        {
            m_strErrMsg = "Output size is NOT initialized, can not set position offset by pixel coordinates!";
            return false;
        }
        if (i32PosOffX > (int32_t)m_u32OutWidth || i32PosOffX < -(int32_t)m_u32OutWidth)
        {
            ostringstream oss; oss << "INVALID argument value PosOffX(" << i32PosOffX << ")! Valid range is [" << -(int32_t)m_u32OutWidth << ", " << m_u32OutWidth << "].";
            m_strErrMsg = oss.str();
            return false;
        }
        if (i32PosOffY > (int32_t)m_u32OutHeight || i32PosOffY < -(int32_t)m_u32OutHeight)
        {
            ostringstream oss; oss << "INVALID argument value PosOffY(" << i32PosOffY << ")! Valid range is [" << -(int32_t)m_u32OutHeight << ", " << m_u32OutHeight << "].";
            m_strErrMsg = oss.str();
            return false;
        }
        const auto fPosOffRatioX = (float)i32PosOffX/(float)m_u32OutWidth;
        const auto fPosOffRatioY = (float)i32PosOffY/(float)m_u32OutHeight;
        return SetPosOffsetRatio(fPosOffRatioX, fPosOffRatioY);
    }

    bool SetPosOffsetX(int32_t i32PosOffX) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (m_i32PosOffsetX == i32PosOffX)
            return true;
        if (m_u32OutWidth == 0 || m_u32OutHeight == 0)
        {
            m_strErrMsg = "Output size is NOT initialized, can not set position offset by pixel coordinates!";
            return false;
        }
        if (i32PosOffX > (int32_t)m_u32OutWidth || i32PosOffX < -(int32_t)m_u32OutWidth)
        {
            ostringstream oss; oss << "INVALID argument value PosOffX(" << i32PosOffX << ")! Valid range is [" << -(int32_t)m_u32OutWidth << ", " << m_u32OutWidth << "].";
            m_strErrMsg = oss.str();
            return false;
        }
        const auto fPosOffRatioX = (float)i32PosOffX/(float)m_u32OutWidth;
        return SetPosOffsetRatioX(fPosOffRatioX);
    }

    int32_t GetPosOffsetX() const override
    { return m_i32PosOffsetX; }

    bool SetPosOffsetY(int32_t i32PosOffY) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (m_i32PosOffsetY == i32PosOffY)
            return true;
        if (m_u32OutWidth == 0 || m_u32OutHeight == 0)
        {
            m_strErrMsg = "Output size is NOT initialized, can not set position offset by pixel coordinates!";
            return false;
        }
        if (i32PosOffY > (int32_t)m_u32OutHeight || i32PosOffY < -(int32_t)m_u32OutHeight)
        {
            ostringstream oss; oss << "INVALID argument value PosOffY(" << i32PosOffY << ")! Valid range is [" << -(int32_t)m_u32OutHeight << ", " << m_u32OutHeight << "].";
            m_strErrMsg = oss.str();
            return false;
        }
        const auto fPosOffRatioY = (float)i32PosOffY/(float)m_u32OutHeight;
        return SetPosOffsetRatioX(fPosOffRatioY);
    }

    int32_t GetPosOffsetY() const override
    { return m_i32PosOffsetY; }

    bool SetPosOffsetRatio(float fPosOffRatioX, float fPosOffRatioY) override
    { return SetPosOffsetRatio(m_tTimeRange.x, fPosOffRatioX, fPosOffRatioY); }

    bool SetPosOffsetRatioX(float fPosOffRatioX) override
    { return SetPosOffsetRatioX(m_tTimeRange.x, fPosOffRatioX); }

    float GetPosOffsetRatioX() const override
    { return m_fPosOffsetRatioX; }

    bool SetPosOffsetRatioY(float fPosOffRatioY) override
    { return SetPosOffsetRatioY(m_tTimeRange.x, fPosOffRatioY); }

    float GetPosOffsetRatioY() const override
    { return m_fPosOffsetRatioY; }

    bool SetPosOffsetRatio(int64_t i64Tick, float fPosOffRatioX, float fPosOffRatioY) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (i64Tick < m_tTimeRange.x || i64Tick > m_tTimeRange.y)
        {
            ostringstream oss; oss << "INVALID argument 'i64Tick'! Argument value " << i64Tick << " is out of the time range [" << m_tTimeRange.x << ", " << m_tTimeRange.y << "]!";
            m_strErrMsg = oss.str();
            return false;
        }
        const auto tMinVal = m_hPosOffsetCurve->GetMinVal();
        const auto tMaxVal = m_hPosOffsetCurve->GetMaxVal();
        if (fPosOffRatioX < tMinVal.x || fPosOffRatioX > tMaxVal.x)
        {
            ostringstream oss; oss << "INVALID argument value PosOffRatioX(" << fPosOffRatioX << ")! Valid range is [" << tMinVal.x << ", " << tMaxVal.x << "].";
            m_strErrMsg = oss.str();
            return false;
        }
        if (fPosOffRatioY < tMinVal.y || fPosOffRatioY > tMaxVal.y)
        {
            ostringstream oss; oss << "INVALID argument value PosOffRatioY(" << fPosOffRatioY << ")! Valid range is [" << tMinVal.y << ", " << tMaxVal.y << "].";
            m_strErrMsg = oss.str();
            return false;
        }
        if (!m_bEnableKeyFramesOnPosOffset)
            i64Tick = m_tTimeRange.x;
        const LibCurve::KeyPoint::ValType tKpVal(fPosOffRatioX, fPosOffRatioY, 0.f, (float)i64Tick);
        const auto iRet = m_hPosOffsetCurve->AddPoint(LibCurve::KeyPoint::CreateInstance(tKpVal), false);
        if (iRet < 0)
        {
            ostringstream oss; oss << "FAILED to invoke 'LibCurve::AddPoint()' to set position offset ratio as (" << tKpVal.x << ", " << tKpVal.y
                    << ") at time tick " << i64Tick << " !";
            m_strErrMsg = oss.str();
            return false;
        }
        return true;
    }

    bool SetPosOffsetRatioX(int64_t i64Tick, float fPosOffRatioX) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (i64Tick < m_tTimeRange.x || i64Tick > m_tTimeRange.y)
        {
            ostringstream oss; oss << "INVALID argument 'i64Tick'! Argument value " << i64Tick << " is out of the time range [" << m_tTimeRange.x << ", " << m_tTimeRange.y << "]!";
            m_strErrMsg = oss.str();
            return false;
        }
        const auto tMinVal = m_hPosOffsetCurve->GetMinVal();
        const auto tMaxVal = m_hPosOffsetCurve->GetMaxVal();
        if (fPosOffRatioX < tMinVal.x || fPosOffRatioX > tMaxVal.x)
        {
            ostringstream oss; oss << "INVALID argument value PosOffRatioX(" << fPosOffRatioX << ")! Valid range is [" << tMinVal.x << ", " << tMaxVal.x << "].";
            m_strErrMsg = oss.str();
            return false;
        }
        if (!m_bEnableKeyFramesOnPosOffset)
            i64Tick = m_tTimeRange.x;
        auto tKpVal = m_hPosOffsetCurve->CalcPointVal((float)i64Tick, false, true);
        tKpVal.x = fPosOffRatioX;
        const auto iRet = m_hPosOffsetCurve->AddPoint(LibCurve::KeyPoint::CreateInstance(tKpVal), false);
        if (iRet < 0)
        {
            ostringstream oss; oss << "FAILED to invoke 'LibCurve::AddPoint()' to set position offset ratio as (" << tKpVal.x << ", " << tKpVal.y
                    << ") at time tick " << i64Tick << " !";
            m_strErrMsg = oss.str();
            return false;
        }
        return true;
    }

    float GetPosOffsetRatioX(int64_t i64Tick) const override
    {
        const auto tKpVal = m_hPosOffsetCurve->CalcPointVal((float)i64Tick, false, true);
        return tKpVal.x;
    }

    bool SetPosOffsetRatioY(int64_t i64Tick, float fPosOffRatioY) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (i64Tick < m_tTimeRange.x || i64Tick > m_tTimeRange.y)
        {
            ostringstream oss; oss << "INVALID argument 'i64Tick'! Argument value " << i64Tick << " is out of the time range [" << m_tTimeRange.x << ", " << m_tTimeRange.y << "]!";
            m_strErrMsg = oss.str();
            return false;
        }
        const auto tMinVal = m_hPosOffsetCurve->GetMinVal();
        const auto tMaxVal = m_hPosOffsetCurve->GetMaxVal();
        if (fPosOffRatioY < tMinVal.y || fPosOffRatioY > tMaxVal.y)
        {
            ostringstream oss; oss << "INVALID argument value PosOffRatioY(" << fPosOffRatioY << ")! Valid range is [" << tMinVal.y << ", " << tMaxVal.y << "].";
            m_strErrMsg = oss.str();
            return false;
        }
        if (!m_bEnableKeyFramesOnPosOffset)
            i64Tick = m_tTimeRange.x;
        auto tKpVal = m_hPosOffsetCurve->CalcPointVal((float)i64Tick, false, true);
        tKpVal.y = fPosOffRatioY;
        const auto iRet = m_hPosOffsetCurve->AddPoint(LibCurve::KeyPoint::CreateInstance(tKpVal), false);
        if (iRet < 0)
        {
            ostringstream oss; oss << "FAILED to invoke 'LibCurve::AddPoint()' to set position offset ratio as (" << tKpVal.x << ", " << tKpVal.y
                    << ") at time tick " << i64Tick << " !";
            m_strErrMsg = oss.str();
            return false;
        }
        return true;
    }

    float GetPosOffsetRatioY(int64_t i64Tick) const override
    {
        const auto tKpVal = m_hPosOffsetCurve->CalcPointVal((float)i64Tick, false, true);
        return tKpVal.y;
    }

    void EnableKeyFramesOnPosOffset(bool bEnable) override
    {
        if (m_bEnableKeyFramesOnPosOffset != bEnable)
        {
            if (!bEnable)
            {
                const auto tHeadKpVal = m_hPosOffsetCurve->CalcPointVal(m_tTimeRange.x, false, true);
                m_hPosOffsetCurve->ClearAll();
                m_hPosOffsetCurve->AddPoint(LibCurve::KeyPoint::CreateInstance(tHeadKpVal), false);
            }
            m_bEnableKeyFramesOnPosOffset = bEnable;
        }
    }

    bool IsKeyFramesEnabledOnPosOffset() const override
    { return m_bEnableKeyFramesOnPosOffset; }

    LibCurve::Curve::Holder GetKeyFramesCurveOnPosOffset() const override
    { return m_hPosOffsetCurve; }

    // Crop
    bool SetCrop(uint32_t u32CropL, uint32_t u32CropT, uint32_t u32CropR, uint32_t u32CropB) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (m_u32CropL == u32CropL && m_u32CropT == u32CropT && m_u32CropR == u32CropR && m_u32CropB == u32CropB)
            return true;
        if (m_u32InWidth > 0 && u32CropL+u32CropR > m_u32InWidth)
        {
            ostringstream oss; oss << "INVALID argument! CropL(" << u32CropL << ") + CropR(" << u32CropR << ") > InWidth(" << m_u32InWidth << ").";
            m_strErrMsg = oss.str();
            return false;
        }
        if (m_u32InHeight > 0 && u32CropT+u32CropB > m_u32InHeight)
        {
            ostringstream oss; oss << "INVALID argument! CropT(" << u32CropT << ") + CropB(" << u32CropB << ") > InHeight(" << m_u32InHeight << ").";
            m_strErrMsg = oss.str();
            return false;
        }
        m_u32CropL = u32CropL;
        m_u32CropT = u32CropT;
        m_u32CropR = u32CropR;
        m_u32CropB = u32CropB;
        m_bNeedUpdateCropRatioParam = true;
        return true;
    }

    bool SetCropL(uint32_t u32CropL) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (m_u32CropL == u32CropL)
            return true;
        if (m_u32InWidth > 0 && u32CropL+m_u32CropR > m_u32InWidth)
        {
            ostringstream oss; oss << "INVALID argument! CropL(" << u32CropL << ") + CropR(" << m_u32CropR << ") > InWidth(" << m_u32InWidth << ").";
            m_strErrMsg = oss.str();
            return false;
        }
        m_u32CropL = u32CropL;
        m_bNeedUpdateCropRatioParam = true;
        return true;
    }

    uint32_t GetCropL() const override
    { return m_u32CropL; }

    bool SetCropT(uint32_t u32CropT) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (m_u32CropT == u32CropT)
            return true;
        if (m_u32InHeight > 0 && u32CropT+m_u32CropB > m_u32InHeight)
        {
            ostringstream oss; oss << "INVALID argument! CropT(" << u32CropT << ") + CropB(" << m_u32CropB << ") > InHeight(" << m_u32InHeight << ").";
            m_strErrMsg = oss.str();
            return false;
        }
        m_u32CropT = u32CropT;
        m_bNeedUpdateCropRatioParam = true;
        return true;
    }

    uint32_t GetCropT() const override
    { return m_u32CropT; }

    bool SetCropR(uint32_t u32CropR) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (m_u32CropR == u32CropR)
            return true;
        if (m_u32InWidth > 0 && m_u32CropL+u32CropR > m_u32InWidth)
        {
            ostringstream oss; oss << "INVALID argument! CropL(" << m_u32CropL << ") + CropR(" << u32CropR << ") > InWidth(" << m_u32InWidth << ").";
            m_strErrMsg = oss.str();
            return false;
        }
        m_u32CropR = u32CropR;
        m_bNeedUpdateCropRatioParam = true;
        return true;
    }

    uint32_t GetCropR() const override
    { return m_u32CropR; }

    bool SetCropB(uint32_t u32CropB) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (m_u32CropB == u32CropB)
            return true;
        if (m_u32InHeight > 0 && m_u32CropT+u32CropB > m_u32InHeight)
        {
            ostringstream oss; oss << "INVALID argument! CropT(" << m_u32CropT << ") + CropB(" << u32CropB << ") > InHeight(" << m_u32InHeight << ").";
            m_strErrMsg = oss.str();
            return false;
        }
        m_u32CropB = u32CropB;
        m_bNeedUpdateCropRatioParam = true;
        return true;
    }

    uint32_t GetCropB() const override
    { return m_u32CropB; }

    bool SetCropRatio(float fCropRatioL, float fCropRatioT, float fCropRatioR, float fCropRatioB) override
    { return SetCropRatio(m_tTimeRange.x, fCropRatioL, fCropRatioT, fCropRatioR, fCropRatioB); }

    bool SetCropRatioL(float fCropRatioL) override
    { return SetCropRatioL(m_tTimeRange.x, fCropRatioL); }

    float GetCropRatioL() const override
    { return m_fCropRatioL; }

    bool SetCropRatioT(float fCropRatioT) override
    { return SetCropRatioT(m_tTimeRange.x, fCropRatioT); }

    float GetCropRatioT() const override
    { return m_fCropRatioT; }

    bool SetCropRatioR(float fCropRatioR) override
    { return SetCropRatioR(m_tTimeRange.x, fCropRatioR); }

    float GetCropRatioR() const override
    { return m_fCropRatioR; }

    bool SetCropRatioB(float fCropRatioB) override
    { return SetCropRatioB(m_tTimeRange.x, fCropRatioB); }

    float GetCropRatioB() const override
    { return m_fCropRatioB; }

    bool SetCropRatio(int64_t i64Tick, float fCropRatioL, float fCropRatioT, float fCropRatioR, float fCropRatioB) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (i64Tick < m_tTimeRange.x || i64Tick > m_tTimeRange.y)
        {
            ostringstream oss; oss << "INVALID argument 'i64Tick'! Argument value " << i64Tick << " is out of the time range [" << m_tTimeRange.x << ", " << m_tTimeRange.y << "]!";
            m_strErrMsg = oss.str();
            return false;
        }
        if (fCropRatioL < 0 || fCropRatioT < 0 || fCropRatioR < 0 || fCropRatioB < 0)
        {
            ostringstream oss; oss << "INVALID argument! CropRatio parameter can NOT be NEGATIVE. CropRatioL(" << fCropRatioL << "), CropRatioT(" << fCropRatioT
                    << "), CropRatioR(" << fCropRatioR << "), CropRatioB(" << fCropRatioB << ").";
            m_strErrMsg = oss.str();
            return false;
        }
        if (fCropRatioL+fCropRatioR > 1.f)
        {
            ostringstream oss; oss << "INVALID argument! CropRatioL(" << fCropRatioL << ") + CropRatioR(" << fCropRatioR << ") > 1.";
            m_strErrMsg = oss.str();
            return false;
        }
        if (fCropRatioT+fCropRatioB > 1.f)
        {
            ostringstream oss; oss << "INVALID argument! CropRatioT(" << fCropRatioT << ") + CropRatioB(" << fCropRatioB << ") > 1.";
            m_strErrMsg = oss.str();
            return false;
        }
        if (!m_bEnableKeyFramesOnCrop)
            i64Tick = m_tTimeRange.x;
        const LibCurve::KeyPoint::ValType tKpVal0(fCropRatioL, fCropRatioT, 0.f, (float)i64Tick);
        auto iRet = m_aCropCurves[0]->AddPoint(LibCurve::KeyPoint::CreateInstance(tKpVal0), false);
        if (iRet < 0)
        {
            ostringstream oss; oss << "FAILED to invoke 'LibCurve::AddPoint()' to set crop ratio (LT) as (" << tKpVal0.x << ", " << tKpVal0.y << ") at time tick " << i64Tick << " !";
            m_strErrMsg = oss.str();
            return false;
        }
        const LibCurve::KeyPoint::ValType tKpVal1(fCropRatioR, fCropRatioB, 0.f, (float)i64Tick);
        iRet = m_aCropCurves[1]->AddPoint(LibCurve::KeyPoint::CreateInstance(tKpVal1), false);
        if (iRet < 0)
        {
            ostringstream oss; oss << "FAILED to invoke 'LibCurve::AddPoint()' to set crop ratio (RB) as (" << tKpVal1.x << ", " << tKpVal1.y << ") at time tick " << i64Tick << " !";
            m_strErrMsg = oss.str();
            return false;
        }
        return true;
    }

    bool SetCropRatioL(int64_t i64Tick, float fCropRatioL) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (i64Tick < m_tTimeRange.x || i64Tick > m_tTimeRange.y)
        {
            ostringstream oss; oss << "INVALID argument 'i64Tick'! Argument value " << i64Tick << " is out of the time range [" << m_tTimeRange.x << ", " << m_tTimeRange.y << "]!";
            m_strErrMsg = oss.str();
            return false;
        }
        if (fCropRatioL < 0)
        {
            ostringstream oss; oss << "INVALID argument vaule CropRatioL(" << fCropRatioL << ")! CropRatio parameter can NOT be NEGATIVE.";
            m_strErrMsg = oss.str();
            return false;
        }
        if (fCropRatioL+m_fCropRatioR > 1.f)
        {
            ostringstream oss; oss << "INVALID argument! CropRatioL(" << fCropRatioL << ") + CropRatioR(" << m_fCropRatioR << ") > 1.";
            m_strErrMsg = oss.str();
            return false;
        }
        if (!m_bEnableKeyFramesOnCrop)
            i64Tick = m_tTimeRange.x;
        auto tKpVal = m_aCropCurves[0]->CalcPointVal((float)i64Tick, false, true);
        tKpVal.x = fCropRatioL;
        const auto iRet = m_aCropCurves[0]->AddPoint(LibCurve::KeyPoint::CreateInstance(tKpVal), false);
        if (iRet < 0)
        {
            ostringstream oss; oss << "FAILED to invoke 'LibCurve::AddPoint()' to set crop ratio (LT) as (" << tKpVal.x << ", " << tKpVal.y << ") at time tick " << i64Tick << " !";
            m_strErrMsg = oss.str();
            return false;
        }
        return true;
    }

    float GetCropRatioL(int64_t i64Tick) const override
    {
        const auto tKpVal = m_aCropCurves[0]->CalcPointVal((float)i64Tick, false, true);
        return tKpVal.x;
    }

    bool SetCropRatioT(int64_t i64Tick, float fCropRatioT) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (i64Tick < m_tTimeRange.x || i64Tick > m_tTimeRange.y)
        {
            ostringstream oss; oss << "INVALID argument 'i64Tick'! Argument value " << i64Tick << " is out of the time range [" << m_tTimeRange.x << ", " << m_tTimeRange.y << "]!";
            m_strErrMsg = oss.str();
            return false;
        }
        if (fCropRatioT < 0)
        {
            ostringstream oss; oss << "INVALID argument vaule CropRatioT(" << fCropRatioT << ")! CropRatio parameter can NOT be NEGATIVE.";
            m_strErrMsg = oss.str();
            return false;
        }
        if (fCropRatioT+m_fCropRatioB > 1.f)
        {
            ostringstream oss; oss << "INVALID argument! CropRatioT(" << fCropRatioT << ") + CropRatioB(" << m_fCropRatioB << ") > 1.";
            m_strErrMsg = oss.str();
            return false;
        }
        if (!m_bEnableKeyFramesOnCrop)
            i64Tick = m_tTimeRange.x;
        auto tKpVal = m_aCropCurves[0]->CalcPointVal((float)i64Tick, false, true);
        tKpVal.y = fCropRatioT;
        const auto iRet = m_aCropCurves[0]->AddPoint(LibCurve::KeyPoint::CreateInstance(tKpVal), false);
        if (iRet < 0)
        {
            ostringstream oss; oss << "FAILED to invoke 'LibCurve::AddPoint()' to set crop ratio (LT) as (" << tKpVal.x << ", " << tKpVal.y << ") at time tick " << i64Tick << " !";
            m_strErrMsg = oss.str();
            return false;
        }
        return true;
    }

    float GetCropRatioT(int64_t i64Tick) const override
    {
        const auto tKpVal = m_aCropCurves[0]->CalcPointVal((float)i64Tick, false, true);
        return tKpVal.y;
    }

    bool SetCropRatioR(int64_t i64Tick, float fCropRatioR) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (i64Tick < m_tTimeRange.x || i64Tick > m_tTimeRange.y)
        {
            ostringstream oss; oss << "INVALID argument 'i64Tick'! Argument value " << i64Tick << " is out of the time range [" << m_tTimeRange.x << ", " << m_tTimeRange.y << "]!";
            m_strErrMsg = oss.str();
            return false;
        }
        if (fCropRatioR < 0)
        {
            ostringstream oss; oss << "INVALID argument vaule CropRatioR(" << fCropRatioR << ")! CropRatio parameter can NOT be NEGATIVE.";
            m_strErrMsg = oss.str();
            return false;
        }
        if (m_fCropRatioL+fCropRatioR > 1.f)
        {
            ostringstream oss; oss << "INVALID argument! CropRatioL(" << m_fCropRatioL << ") + CropRatioR(" << fCropRatioR << ") > 1.";
            m_strErrMsg = oss.str();
            return false;
        }
        if (!m_bEnableKeyFramesOnCrop)
            i64Tick = m_tTimeRange.x;
        auto tKpVal = m_aCropCurves[1]->CalcPointVal((float)i64Tick, false, true);
        tKpVal.x = fCropRatioR;
        const auto iRet = m_aCropCurves[1]->AddPoint(LibCurve::KeyPoint::CreateInstance(tKpVal), false);
        if (iRet < 0)
        {
            ostringstream oss; oss << "FAILED to invoke 'LibCurve::AddPoint()' to set crop ratio (RB) as (" << tKpVal.x << ", " << tKpVal.y << ") at time tick " << i64Tick << " !";
            m_strErrMsg = oss.str();
            return false;
        }
        return true;
    }

    float GetCropRatioR(int64_t i64Tick) const override
    {
        const auto tKpVal = m_aCropCurves[1]->CalcPointVal((float)i64Tick, false, true);
        return tKpVal.x;
    }

    bool SetCropRatioB(int64_t i64Tick, float fCropRatioB) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (i64Tick < m_tTimeRange.x || i64Tick > m_tTimeRange.y)
        {
            ostringstream oss; oss << "INVALID argument 'i64Tick'! Argument value " << i64Tick << " is out of the time range [" << m_tTimeRange.x << ", " << m_tTimeRange.y << "]!";
            m_strErrMsg = oss.str();
            return false;
        }
        if (fCropRatioB < 0)
        {
            ostringstream oss; oss << "INVALID argument vaule CropRatioB(" << fCropRatioB << ")! CropRatio parameter can NOT be NEGATIVE.";
            m_strErrMsg = oss.str();
            return false;
        }
        if (m_fCropRatioT+fCropRatioB > 1.f)
        {
            ostringstream oss; oss << "INVALID argument! CropRatioT(" << m_fCropRatioT << ") + CropRatioB(" << fCropRatioB << ") > 1.";
            m_strErrMsg = oss.str();
            return false;
        }
        if (!m_bEnableKeyFramesOnCrop)
            i64Tick = m_tTimeRange.x;
        auto tKpVal = m_aCropCurves[1]->CalcPointVal((float)i64Tick, false, true);
        tKpVal.y = fCropRatioB;
        const auto iRet = m_aCropCurves[1]->AddPoint(LibCurve::KeyPoint::CreateInstance(tKpVal), false);
        if (iRet < 0)
        {
            ostringstream oss; oss << "FAILED to invoke 'LibCurve::AddPoint()' to set crop ratio (RB) as (" << tKpVal.x << ", " << tKpVal.y << ") at time tick " << i64Tick << " !";
            m_strErrMsg = oss.str();
            return false;
        }
        return true;
    }

    float GetCropRatioB(int64_t i64Tick) const override
    {
        const auto tKpVal = m_aCropCurves[1]->CalcPointVal((float)i64Tick, false, true);
        return tKpVal.y;
    }

    void EnableKeyFramesOnCrop(bool bEnable) override
    {
        if (m_bEnableKeyFramesOnCrop != bEnable)
        {
            if (!bEnable)
            {
                const auto tHeadKpVal0 = m_aCropCurves[0]->CalcPointVal(m_tTimeRange.x, false, true);
                m_aCropCurves[0]->ClearAll();
                m_aCropCurves[0]->AddPoint(LibCurve::KeyPoint::CreateInstance(tHeadKpVal0), false);
                const auto tHeadKpVal1 = m_aCropCurves[1]->CalcPointVal(m_tTimeRange.x, false, true);
                m_aCropCurves[1]->ClearAll();
                m_aCropCurves[1]->AddPoint(LibCurve::KeyPoint::CreateInstance(tHeadKpVal1), false);
            }
            m_bEnableKeyFramesOnCrop = bEnable;
        }
    }

    bool IsKeyFramesEnabledOnCrop() const override
    { return m_bEnableKeyFramesOnCrop; }

    vector<LibCurve::Curve::Holder> GetKeyFramesCurveOnCrop() const override
    { return m_aCropCurves; }

    // Scale
    bool SetScale(float fScaleX, float fScaleY) override
    { return SetScale(m_tTimeRange.x, fScaleX, fScaleY); }

    bool SetScaleX(float fScaleX) override
    { return SetScaleX(m_tTimeRange.x, fScaleX); }

    float GetScaleX() const override
    { return m_fScaleX; }

    bool SetScaleY(float fScaleY) override
    { return SetScaleY(m_tTimeRange.x, fScaleY); }

    float GetScaleY() const override
    { return m_fScaleY; }

    bool SetScale(int64_t i64Tick, float fScaleX, float fScaleY) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (m_fScaleX == fScaleX && m_fScaleY == fScaleY)
            return true;
        const auto tMinVal = m_hScaleCurve->GetMinVal();
        const auto tMaxVal = m_hScaleCurve->GetMaxVal();
        if (fScaleX < tMinVal.x || fScaleX > tMaxVal.x)
        {
            ostringstream oss; oss << "INVALID argument value ScaleX(" << fScaleX << ")! Valid range is [" << tMinVal.x << ", " << tMaxVal.x << "].";
            m_strErrMsg = oss.str();
            return false;
        }
        if (fScaleY < tMinVal.x || fScaleY > tMaxVal.x)
        {
            ostringstream oss; oss << "INVALID argument value ScaleY(" << fScaleY << ")! Valid range is [" << tMinVal.y << ", " << tMaxVal.y << "].";
            m_strErrMsg = oss.str();
            return false;
        }
        if (!m_bEnableKeyFramesOnScale)
            i64Tick = m_tTimeRange.x;
        const LibCurve::KeyPoint::ValType tKpVal(fScaleX, fScaleY, 0.f, (float)i64Tick);
        auto iRet = m_hScaleCurve->AddPoint(LibCurve::KeyPoint::CreateInstance(tKpVal), false);
        if (iRet < 0)
        {
            ostringstream oss; oss << "FAILED to invoke 'LibCurve::AddPoint()' to set scale as (" << tKpVal.x << ", " << tKpVal.y << ") at time tick " << i64Tick << " !";
            m_strErrMsg = oss.str();
            return false;
        }
        return true;
    }

    bool SetScaleX(int64_t i64Tick, float fScaleX) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (m_fScaleX == fScaleX)
            return true;
        const auto tMinVal = m_hScaleCurve->GetMinVal();
        const auto tMaxVal = m_hScaleCurve->GetMaxVal();
        if (fScaleX < tMinVal.x || fScaleX > tMaxVal.x)
        {
            ostringstream oss; oss << "INVALID argument value ScaleX(" << fScaleX << ")! Valid range is [" << tMinVal.x << ", " << tMaxVal.x << "].";
            m_strErrMsg = oss.str();
            return false;
        }
        if (!m_bEnableKeyFramesOnScale)
            i64Tick = m_tTimeRange.x;
        auto tKpVal = m_hScaleCurve->CalcPointVal((float)i64Tick, false, true);
        tKpVal.x = fScaleX;
        auto iRet = m_hScaleCurve->AddPoint(LibCurve::KeyPoint::CreateInstance(tKpVal), false);
        if (iRet < 0)
        {
            ostringstream oss; oss << "FAILED to invoke 'LibCurve::AddPoint()' to set scale as (" << tKpVal.x << ", " << tKpVal.y << ") at time tick " << i64Tick << " !";
            m_strErrMsg = oss.str();
            return false;
        }
        return true;
    }

    float GetScaleX(int64_t i64Tick) const override
    {
        const auto tKpVal = m_hScaleCurve->CalcPointVal((float)i64Tick, false, true);
        return tKpVal.x;
    }

    bool SetScaleY(int64_t i64Tick, float fScaleY) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (m_fScaleY == fScaleY)
            return true;
        const auto tMinVal = m_hScaleCurve->GetMinVal();
        const auto tMaxVal = m_hScaleCurve->GetMaxVal();
        if (fScaleY < tMinVal.y || fScaleY > tMaxVal.y)
        {
            ostringstream oss; oss << "INVALID argument value ScaleY(" << fScaleY << ")! Valid range is [" << tMinVal.y << ", " << tMaxVal.y << "].";
            m_strErrMsg = oss.str();
            return false;
        }
        if (!m_bEnableKeyFramesOnScale)
            i64Tick = m_tTimeRange.x;
        auto tKpVal = m_hScaleCurve->CalcPointVal((float)i64Tick, false, true);
        tKpVal.y = fScaleY;
        auto iRet = m_hScaleCurve->AddPoint(LibCurve::KeyPoint::CreateInstance(tKpVal), false);
        if (iRet < 0)
        {
            ostringstream oss; oss << "FAILED to invoke 'LibCurve::AddPoint()' to set scale as (" << tKpVal.x << ", " << tKpVal.y << ") at time tick " << i64Tick << " !";
            m_strErrMsg = oss.str();
            return false;
        }
        return true;
    }

    float GetScaleY(int64_t i64Tick) const override
    {
        const auto tKpVal = m_hScaleCurve->CalcPointVal((float)i64Tick, false, true);
        return tKpVal.y;
    }

    void SetKeepAspectRatio(bool bEnable) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (m_bKeepAspectRatio != bEnable)
        {
            if (m_fScaleX != m_fScaleY)
                m_bNeedUpdateScaleParam = true;
            m_bKeepAspectRatio = bEnable;
        }
    }

    bool IsKeepAspectRatio() const override
    { return m_bKeepAspectRatio; }

    void EnableKeyFramesOnScale(bool bEnable) override
    {
        if (m_bEnableKeyFramesOnScale != bEnable)
        {
            if (!bEnable)
            {
                const auto tHeadKpVal = m_hScaleCurve->CalcPointVal(m_tTimeRange.x, false, true);
                m_hScaleCurve->ClearAll();
                m_hScaleCurve->AddPoint(LibCurve::KeyPoint::CreateInstance(tHeadKpVal), false);
            }
            m_bEnableKeyFramesOnScale = bEnable;
        }
    }

    bool IsKeyFramesEnabledOnScale() const override
    { return m_bEnableKeyFramesOnScale; }

    LibCurve::Curve::Holder GetKeyFramesCurveOnScale() const override
    { return m_hScaleCurve; }

    // Rotation
    bool SetRotation(float fAngle) override
    { return SetRotation(m_tTimeRange.x, fAngle); }

    float GetRotation() const override
    { return m_fRotateAngle; }

    bool SetRotation(int64_t i64Tick, float fAngle) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        int32_t n = (int32_t)trunc(fAngle/360);
        fAngle -= n*360;
        if (m_fRotateAngle == fAngle)
            return true;
        const auto tMinVal = m_hRotationCurve->GetMinVal();
        const auto tMaxVal = m_hRotationCurve->GetMaxVal();
        if (fAngle < tMinVal.x || fAngle > tMaxVal.x)
        {
            ostringstream oss; oss << "INVALID argument value Rotation(" << fAngle << ")! Valid range is [" << tMinVal.y << ", " << tMaxVal.y << "].";
            m_strErrMsg = oss.str();
            return false;
        }
        if (!m_bEnableKeyFramesOnRotation)
            i64Tick = m_tTimeRange.x;
        const LibCurve::KeyPoint::ValType tKpVal(fAngle, 0.f, 0.f, (float)i64Tick);
        auto iRet = m_hRotationCurve->AddPoint(LibCurve::KeyPoint::CreateInstance(tKpVal), false);
        if (iRet < 0)
        {
            ostringstream oss; oss << "FAILED to invoke 'LibCurve::AddPoint()' to set rotation as (" << tKpVal.x << ") at time tick " << i64Tick << " !";
            m_strErrMsg = oss.str();
            return false;
        }
        return true;
    }

    float GetRotation(int64_t i64Tick) const override
    {
        const auto tKpVal = m_hRotationCurve->CalcPointVal((float)i64Tick, false, true);
        return tKpVal.x;
    }

    void EnableKeyFramesOnRotation(bool bEnable) override
    {
        if (m_bEnableKeyFramesOnRotation != bEnable)
        {
            if (!bEnable)
            {
                const auto tHeadKpVal = m_hRotationCurve->CalcPointVal(m_tTimeRange.x, false, true);
                m_hRotationCurve->ClearAll();
                m_hRotationCurve->AddPoint(LibCurve::KeyPoint::CreateInstance(tHeadKpVal), false);
            }
            m_bEnableKeyFramesOnRotation = bEnable;
        }
    }

    bool IsKeyFramesEnabledOnRotation() const override
    { return m_bEnableKeyFramesOnRotation; }

    LibCurve::Curve::Holder GetKeyFramesCurveOnRotation() const override
    { return m_hRotationCurve; }

    // Opacity
    bool SetOpacity(float opacity) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        m_fOpacity = opacity;
        return true;
    }

    float GetOpacity() const override
    { return m_fOpacity; }

    bool SetOpacity(int64_t i64Tick, float fOpacity) override
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (m_fOpacity == fOpacity)
            return true;
        const auto tMinVal = m_hOpacityCurve->GetMinVal();
        const auto tMaxVal = m_hOpacityCurve->GetMaxVal();
        if (fOpacity < tMinVal.x || fOpacity > tMaxVal.x)
        {
            ostringstream oss; oss << "INVALID argument value Opacity(" << fOpacity << ")! Valid range is [" << tMinVal.y << ", " << tMaxVal.y << "].";
            m_strErrMsg = oss.str();
            return false;
        }
        if (!m_bEnableKeyFramesOnOpacity)
            i64Tick = m_tTimeRange.x;
        const LibCurve::KeyPoint::ValType tKpVal(fOpacity, 0.f, 0.f, (float)i64Tick);
        auto iRet = m_hOpacityCurve->AddPoint(LibCurve::KeyPoint::CreateInstance(tKpVal), false);
        if (iRet < 0)
        {
            ostringstream oss; oss << "FAILED to invoke 'LibCurve::AddPoint()' to set opacity as (" << tKpVal.x << ") at time tick " << i64Tick << " !";
            m_strErrMsg = oss.str();
            return false;
        }
        return true;
    }

    float GetOpacity(int64_t i64Tick) const override
    {
        const auto tKpVal = m_hOpacityCurve->CalcPointVal((float)i64Tick, false, true);
        return tKpVal.x;
    }

    void EnableKeyFramesOnOpacity(bool bEnable) override
    {
        if (m_bEnableKeyFramesOnOpacity != bEnable)
        {
            if (!bEnable)
            {
                const auto tHeadKpVal = m_hOpacityCurve->CalcPointVal(m_tTimeRange.x, false, true);
                m_hOpacityCurve->ClearAll();
                m_hOpacityCurve->AddPoint(LibCurve::KeyPoint::CreateInstance(tHeadKpVal), false);
            }
            m_bEnableKeyFramesOnOpacity = bEnable;
        }
    }

    bool IsKeyFramesEnabledOnOpacity() const override
    { return m_bEnableKeyFramesOnOpacity; }

    LibCurve::Curve::Holder GetKeyFramesCurveOnOpacity() const override
    { return m_hOpacityCurve; }

    imgui_json::value SaveAsJson() const override
    {
        imgui_json::value j;
        j["output_format"] = imgui_json::string(m_strOutputFormat);
        j["aspect_fit_type"] = imgui_json::number((int)m_eAspectFitType);
        j["pos_offset_curve"] = m_hPosOffsetCurve->SaveAsJson();
        j["pos_offset_keyframes_enabled"] = m_bEnableKeyFramesOnPosOffset;
        j["crop_lt_curve"] = m_aCropCurves[0]->SaveAsJson();
        j["crop_rb_curve"] = m_aCropCurves[1]->SaveAsJson();
        j["crop_keyframes_enabled"] = m_bEnableKeyFramesOnCrop;
        j["scale_curve"] = m_hScaleCurve->SaveAsJson();
        j["keep_aspect_ratio"] = m_bKeepAspectRatio;
        j["scale_keyframes_enabled"] = m_bEnableKeyFramesOnScale;
        j["rotation_curve"] = m_hRotationCurve->SaveAsJson();
        j["rotation_keyframes_enabled"] = m_bEnableKeyFramesOnRotation;
        j["opacity_curve"] = m_hOpacityCurve->SaveAsJson();
        j["opacity_keyframes_enabled"] = m_bEnableKeyFramesOnOpacity;
        return move(j);
    }

    bool LoadFromJson(const imgui_json::value& j) override
    {
        string strAttrName;
        strAttrName = "output_format";
        if (j.contains(strAttrName) && j[strAttrName].is_string())
        {
            if (!SetOutputFormat(j[strAttrName].get<imgui_json::string>()))
                return false;
        }
        strAttrName = "aspect_fit_type";
        if (j.contains(strAttrName) && j[strAttrName].is_number())
        {
            if (!SetAspectFitType((AspectFitType)j[strAttrName].get<imgui_json::number>()))
                return false;
        }
        strAttrName = "pos_offset_curve";
        if (j.contains(strAttrName) && j[strAttrName].is_object())
            m_hPosOffsetCurve->LoadFromJson(j[strAttrName]);
        strAttrName = "pos_offset_keyframes_enabled";
        if (j.contains(strAttrName) && j[strAttrName].is_boolean())
            m_bEnableKeyFramesOnPosOffset = j[strAttrName].get<imgui_json::boolean>();
        strAttrName = "crop_lt_curve";
        if (j.contains(strAttrName) && j[strAttrName].is_object())
            m_aCropCurves[0]->LoadFromJson(j[strAttrName]);
        strAttrName = "crop_rb_curve";
        if (j.contains(strAttrName) && j[strAttrName].is_object())
            m_aCropCurves[1]->LoadFromJson(j[strAttrName]);
        strAttrName = "crop_keyframes_enabled";
        if (j.contains(strAttrName) && j[strAttrName].is_boolean())
            m_bEnableKeyFramesOnCrop = j[strAttrName].get<imgui_json::boolean>();
        strAttrName = "scale_curve";
        if (j.contains(strAttrName) && j[strAttrName].is_object())
            m_hScaleCurve->LoadFromJson(j[strAttrName]);
        strAttrName = "keep_aspect_ratio";
        if (j.contains(strAttrName) && j[strAttrName].is_boolean())
            m_bKeepAspectRatio = j[strAttrName].get<imgui_json::boolean>();
        strAttrName = "scale_keyframes_enabled";
        if (j.contains(strAttrName) && j[strAttrName].is_boolean())
            m_bEnableKeyFramesOnScale = j[strAttrName].get<imgui_json::boolean>();
        strAttrName = "rotation_curve";
        if (j.contains(strAttrName) && j[strAttrName].is_object())
            m_hRotationCurve->LoadFromJson(j[strAttrName]);
        strAttrName = "rotation_keyframes_enabled";
        if (j.contains(strAttrName) && j[strAttrName].is_boolean())
            m_bEnableKeyFramesOnRotation = j[strAttrName].get<imgui_json::boolean>();
        strAttrName = "opacity_curve";
        if (j.contains(strAttrName) && j[strAttrName].is_object())
            m_hOpacityCurve->LoadFromJson(j[strAttrName]);
        strAttrName = "opacity_keyframes_enabled";
        if (j.contains(strAttrName) && j[strAttrName].is_boolean())
            m_bEnableKeyFramesOnOpacity = j[strAttrName].get<imgui_json::boolean>();
        return true;
    }

    string GetError() const override
    { return m_strErrMsg; }

protected:
    bool UpdateParamsByKeyFrames(int64_t i64Tick)
    {
        float fTick;
        LibCurve::KeyPoint::ValType tKpVal;
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        // Position offset
        fTick = m_bEnableKeyFramesOnPosOffset ? (float)i64Tick : m_tTimeRange.x;
        tKpVal = m_hPosOffsetCurve->CalcPointVal(fTick, false, true);
        const auto i32PosOffX = (int32_t)round((float)m_u32OutWidth*tKpVal.x);
        const auto i32PosOffY = (int32_t)round((float)m_u32OutHeight*tKpVal.y);
        if (i32PosOffX != m_i32PosOffsetX || i32PosOffY != m_i32PosOffsetY)
        {
            m_i32PosOffsetX = i32PosOffX;
            m_i32PosOffsetY = i32PosOffY;
            m_fPosOffsetRatioX = tKpVal.x;
            m_fPosOffsetRatioY = tKpVal.y;
            m_bNeedUpdatePosOffsetParam = true;
        }
        // Crop
        if (m_bNeedUpdateCropRatioParam)
        {
            const auto fCropRatioL = (float)m_u32CropL/m_u32InWidth;
            const auto fCropRatioT = (float)m_u32CropT/m_u32InHeight;
            const auto fCropRatioR = (float)m_u32CropR/m_u32InWidth;
            const auto fCropRatioB = (float)m_u32CropB/m_u32InHeight;
            m_bNeedUpdateCropRatioParam = false;
            if (!SetCropRatio(fCropRatioL, fCropRatioT, fCropRatioR, fCropRatioB))
                return false;
        }
        fTick = m_bEnableKeyFramesOnCrop ? (float)i64Tick : m_tTimeRange.x;
        tKpVal = m_aCropCurves[0]->CalcPointVal(fTick, false, true);
        const auto u32CropL = (uint32_t)round((float)m_u32InWidth*tKpVal.x);
        const auto u32CropT = (uint32_t)round((float)m_u32InHeight*tKpVal.y);
        m_fCropRatioL = tKpVal.x; m_fCropRatioT = tKpVal.y;
        tKpVal = m_aCropCurves[1]->CalcPointVal(fTick, false, true);
        const auto u32CropR = (uint32_t)round((float)m_u32InWidth*tKpVal.x);
        const auto u32CropB = (uint32_t)round((float)m_u32InHeight*tKpVal.y);
        m_fCropRatioR = tKpVal.x; m_fCropRatioB = tKpVal.y;
        if (u32CropL != m_u32CropL || u32CropT != m_u32CropT || u32CropR != m_u32CropR || u32CropB != m_u32CropB)
        {
            m_u32CropL = u32CropL;
            m_u32CropT = u32CropT;
            m_u32CropR = u32CropR;
            m_u32CropB = u32CropB;
            m_bNeedUpdateCropParam = true;
        }
        // Scale
        fTick = m_bEnableKeyFramesOnScale ? (float)i64Tick : m_tTimeRange.x;
        tKpVal = m_hScaleCurve->CalcPointVal(fTick, false, true);
        if (tKpVal.x != m_fScaleX || (!m_bKeepAspectRatio && tKpVal.y != m_fScaleY))
        {
            m_fScaleX = tKpVal.x;
            m_fScaleY = tKpVal.y;
            m_bNeedUpdateScaleParam = true;
        }
        // Rotation
        fTick = m_bEnableKeyFramesOnRotation ? (float)i64Tick : m_tTimeRange.x;
        tKpVal = m_hRotationCurve->CalcPointVal(fTick, false, true);
        if (tKpVal.x != m_fRotateAngle)
        {
            m_fRotateAngle = tKpVal.x;
            m_bNeedUpdateRotationParam = true;
        }
        // Opacity
        fTick = m_bEnableKeyFramesOnOpacity ? (float)i64Tick : m_tTimeRange.x;
        tKpVal = m_hOpacityCurve->CalcPointVal(fTick, false, true);
        if (tKpVal.x != m_fRotateAngle)
        {
            m_fOpacity = tKpVal.x;
        }

        return true;
    }

protected:
    recursive_mutex m_mtxProcessLock;
    uint32_t m_u32InWidth{0}, m_u32InHeight{0};
    uint32_t m_u32OutWidth{0}, m_u32OutHeight{0};
    string m_strOutputFormat;
    AspectFitType m_eAspectFitType{ASPECT_FIT_TYPE__FIT};
    MatUtils::Vec2<int64_t> m_tTimeRange;
    int32_t m_i32PosOffsetX{0}, m_i32PosOffsetY{0};
    float m_fPosOffsetRatioX{0}, m_fPosOffsetRatioY{0};
    uint32_t m_u32CropL{0}, m_u32CropR{0}, m_u32CropT{0}, m_u32CropB{0};
    float m_fCropRatioL{0}, m_fCropRatioR{0}, m_fCropRatioT{0}, m_fCropRatioB{0};
    float m_fScaleX{1}, m_fScaleY{1};
    bool m_bKeepAspectRatio{false};
    float m_fRotateAngle{0};
    float m_fOpacity{1.f};
    bool m_bNeedUpdatePosOffsetParam{false};
    bool m_bNeedUpdateCropParam{false};
    bool m_bNeedUpdateCropRatioParam{false};
    bool m_bNeedUpdateRotationParam{false};
    bool m_bNeedUpdateScaleParam{true};
    LibCurve::Curve::Holder m_hPosOffsetCurve;
    bool m_bEnableKeyFramesOnPosOffset{false};
    vector<LibCurve::Curve::Holder> m_aCropCurves;
    bool m_bEnableKeyFramesOnCrop{false};
    LibCurve::Curve::Holder m_hScaleCurve;
    bool m_bEnableKeyFramesOnScale{false};
    LibCurve::Curve::Holder m_hRotationCurve;
    bool m_bEnableKeyFramesOnRotation{false};
    LibCurve::Curve::Holder m_hOpacityCurve;
    bool m_bEnableKeyFramesOnOpacity{false};
    string m_strErrMsg;
};
}