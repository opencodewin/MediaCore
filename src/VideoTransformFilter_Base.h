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

namespace MediaCore
{
class VideoTransformFilter_Base : public VideoTransformFilter
{
public:
    VideoTransformFilter_Base()
    {
        m_hPosOffsetCurve = ImGui::ImNewCurve::Curve::CreateInstance("PosOffsetCurve", ImGui::ImNewCurve::Linear, {0,0,0,0}, {1,1,1,0}, {0,0,0,0});
    }

    virtual ~VideoTransformFilter_Base() {}

    Holder Clone(SharedSettings::Holder hSettings) override
    {
        VideoTransformFilter::Holder newInstance = VideoTransformFilter::CreateInstance();
        if (!newInstance->Initialize(hSettings))
            return nullptr;
        newInstance->SetScaleType(GetScaleType());
        newInstance->SetScaleX(GetScaleX());
        newInstance->SetScaleY(GetScaleY());
        newInstance->SetPosOffsetX(GetPosOffsetX());
        newInstance->SetPosOffsetY(GetPosOffsetY());
        newInstance->SetRotation(GetRotation());
        newInstance->SetCropL(GetCropL());
        newInstance->SetCropT(GetCropT());
        newInstance->SetCropR(GetCropR());
        newInstance->SetCropB(GetCropB());
        newInstance->SetPosOffsetRatioX(GetPosOffsetRatioX());
        newInstance->SetPosOffsetRatioY(GetPosOffsetRatioY());
        newInstance->SetCropRatioL(GetCropRatioL());
        newInstance->SetCropRatioT(GetCropRatioT());
        newInstance->SetCropRatioR(GetCropRatioR());
        newInstance->SetCropRatioB(GetCropRatioB());
        newInstance->SetKeyPoint(*GetKeyPoint());
        return newInstance;
    }

    uint32_t GetInWidth() const override
    { return m_u32InWidth; }

    uint32_t GetInHeight() const override
    { return m_u32InHeight; }

    uint32_t GetOutWidth() const override
    { return m_u32OutWidth; }

    uint32_t GetOutHeight() const override
    { return m_u32OutHeight; }

    std::string GetOutputFormat() const override
    { return m_strOutputFormat; }

    bool SetScaleType(ScaleType type) override
    {
        std::lock_guard<std::recursive_mutex> lk(m_mtxProcessLock);
        if (type < SCALE_TYPE__FIT || type > SCALE_TYPE__STRETCH)
        {
            m_strErrMsg = "INVALID argument 'type'!";
            return false;
        }
        if (m_eScaleType == type)
            return true;
        m_eScaleType = type;
        m_bNeedUpdateScaleParam = true;
        return true;
    }

    ScaleType GetScaleType() const override
    { return m_eScaleType; }

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
        std::lock_guard<std::recursive_mutex> lk(m_mtxProcessLock);
        if (m_i32PosOffsetX == i32PosOffX && m_i32PosOffsetY == i32PosOffY)
            return true;
        if (m_u32OutWidth == 0 || m_u32OutHeight == 0)
        {
            m_strErrMsg = "Output size is NOT initialized, can not set position offset by pixel coordinates!";
            return false;
        }
        const auto fPosOffRatioX = (float)i32PosOffX/(float)m_u32OutWidth;
        const auto fPosOffRatioY = (float)i32PosOffY/(float)m_u32OutHeight;
        return SetPosOffsetRatio(fPosOffRatioX, fPosOffRatioY);
    }

    bool SetPosOffsetX(int32_t i32PosOffX) override
    {
        std::lock_guard<std::recursive_mutex> lk(m_mtxProcessLock);
        if (m_i32PosOffsetX == i32PosOffX)
            return true;
        if (m_u32OutWidth == 0 || m_u32OutHeight == 0)
        {
            m_strErrMsg = "Output size is NOT initialized, can not set position offset by pixel coordinates!";
            return false;
        }
        const auto fPosOffRatioX = (float)i32PosOffX/(float)m_u32OutWidth;
        return SetPosOffsetRatioX(fPosOffRatioX);
    }

    int32_t GetPosOffsetX() const override
    { return m_i32PosOffsetX; }

    bool SetPosOffsetY(int32_t i32PosOffY) override
    {
        std::lock_guard<std::recursive_mutex> lk(m_mtxProcessLock);
        if (m_i32PosOffsetY == i32PosOffY)
            return true;
        if (m_u32OutWidth == 0 || m_u32OutHeight == 0)
        {
            m_strErrMsg = "Output size is NOT initialized, can not set position offset by pixel coordinates!";
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
        std::lock_guard<std::recursive_mutex> lk(m_mtxProcessLock);
        if (i64Tick < m_tTimeRange.x || i64Tick > m_tTimeRange.y)
        {
            std::ostringstream oss; oss << "INVALID argument 'i64Tick'! Argument value " << i64Tick << " is out of the time range [" << m_tTimeRange.x << ", " << m_tTimeRange.y << "]!";
            m_strErrMsg = oss.str();
            return false;
        }
        if (!m_bEnablePosOffsetKeyFrames)
            i64Tick = m_tTimeRange.x;
        const ImGui::ImNewCurve::KeyPoint::ValType tKpVal(fPosOffRatioX, fPosOffRatioY, 0.f, (float)i64Tick);
        auto hNewKp = ImGui::ImNewCurve::KeyPoint::CreateInstance(tKpVal, m_hPosOffsetCurve->GetCurveType());
        const auto iRet = m_hPosOffsetCurve->AddPoint(hNewKp, false);
        if (iRet < 0)
        {
            std::ostringstream oss; oss << "FAILED to invoke 'ImNewCurve::AddPoint()' to set position offset ratio as (" << tKpVal.x << ", " << tKpVal.y
                    << ") at time tick " << i64Tick << " !";
            m_strErrMsg = oss.str();
            return false;
        }
        return true;
    }

    bool SetPosOffsetRatioX(int64_t i64Tick, float fPosOffRatioX) override
    {
        std::lock_guard<std::recursive_mutex> lk(m_mtxProcessLock);
        if (i64Tick < m_tTimeRange.x || i64Tick > m_tTimeRange.y)
        {
            std::ostringstream oss; oss << "INVALID argument 'i64Tick'! Argument value " << i64Tick << " is out of the time range [" << m_tTimeRange.x << ", " << m_tTimeRange.y << "]!";
            m_strErrMsg = oss.str();
            return false;
        }
        if (!m_bEnablePosOffsetKeyFrames)
            i64Tick = m_tTimeRange.x;
        auto tKpVal = m_hPosOffsetCurve->CalcPointVal((float)i64Tick, false, true);
        tKpVal.x = fPosOffRatioX;
        auto hNewKp = ImGui::ImNewCurve::KeyPoint::CreateInstance(tKpVal, m_hPosOffsetCurve->GetCurveType());
        const auto iRet = m_hPosOffsetCurve->AddPoint(hNewKp, false);
        if (iRet < 0)
        {
            std::ostringstream oss; oss << "FAILED to invoke 'ImNewCurve::AddPoint()' to set position offset ratio as (" << tKpVal.x << ", " << tKpVal.y
                    << ") at time tick " << i64Tick << " !";
            m_strErrMsg = oss.str();
            return false;
        }
        return true;
    }

    float GetPosOffsetRatioX(int64_t i64Tick) const override
    { return m_fPosOffsetRatioX; }

    bool SetPosOffsetRatioY(int64_t i64Tick, float fPosOffRatioY) override
    {
        std::lock_guard<std::recursive_mutex> lk(m_mtxProcessLock);
        if (i64Tick < m_tTimeRange.x || i64Tick > m_tTimeRange.y)
        {
            std::ostringstream oss; oss << "INVALID argument 'i64Tick'! Argument value " << i64Tick << " is out of the time range [" << m_tTimeRange.x << ", " << m_tTimeRange.y << "]!";
            m_strErrMsg = oss.str();
            return false;
        }
        if (!m_bEnablePosOffsetKeyFrames)
            i64Tick = m_tTimeRange.x;
        auto tKpVal = m_hPosOffsetCurve->CalcPointVal((float)i64Tick, false, true);
        tKpVal.y = fPosOffRatioY;
        auto hNewKp = ImGui::ImNewCurve::KeyPoint::CreateInstance(tKpVal, m_hPosOffsetCurve->GetCurveType());
        const auto iRet = m_hPosOffsetCurve->AddPoint(hNewKp, false);
        if (iRet < 0)
        {
            std::ostringstream oss; oss << "FAILED to invoke 'ImNewCurve::AddPoint()' to set position offset ratio as (" << tKpVal.x << ", " << tKpVal.y
                    << ") at time tick " << i64Tick << " !";
            m_strErrMsg = oss.str();
            return false;
        }
        return true;
    }

    float GetPosOffsetRatioY(int64_t i64Tick) const override
    { return m_fPosOffsetRatioY; }

    void EnablePosOffsetKeyFrames(bool bEnable) override
    {
        if (m_bEnablePosOffsetKeyFrames != bEnable)
        {
            if (!bEnable)
            {
                const auto tHeadKpVal = m_hPosOffsetCurve->CalcPointVal(m_tTimeRange.x, false, true);
                m_hPosOffsetCurve->ClearAll();
                m_hPosOffsetCurve->AddPoint(ImGui::ImNewCurve::KeyPoint::CreateInstance(tHeadKpVal, m_hPosOffsetCurve->GetCurveType()), false);
            }
            m_bEnablePosOffsetKeyFrames = bEnable;
        }
    }

    bool IsPosOffsetKeyFramesEnabled() const override
    { return m_bEnablePosOffsetKeyFrames; }

    // Crop
    bool SetCrop(uint32_t u32CropL, uint32_t u32CropT, uint32_t u32CropR, uint32_t u32CropB) override
    {
        std::lock_guard<std::recursive_mutex> lk(m_mtxProcessLock);
        if (m_u32CropL == u32CropL && m_u32CropT == u32CropT && m_u32CropR == u32CropR && m_u32CropB == u32CropB)
            return true;
        m_u32CropL = u32CropL;
        m_u32CropT = u32CropT;
        m_u32CropR = u32CropR;
        m_u32CropB = u32CropB;
        m_bNeedUpdateCropParam = true;
        return true;
    }

    bool SetCropL(uint32_t u32CropL) override
    {
        std::lock_guard<std::recursive_mutex> lk(m_mtxProcessLock);
        if (m_u32CropL == u32CropL)
            return true;
        m_u32CropL = u32CropL;
        m_bNeedUpdateCropParam = true;
        return true;
    }

    uint32_t GetCropL() const override
    { return m_u32CropL; }

    bool SetCropT(uint32_t u32CropT) override
    {
        std::lock_guard<std::recursive_mutex> lk(m_mtxProcessLock);
        if (m_u32CropT == u32CropT)
            return true;
        m_u32CropT = u32CropT;
        m_bNeedUpdateCropParam = true;
        return true;
    }

    uint32_t GetCropT() const override
    { return m_u32CropT; }

    bool SetCropR(uint32_t u32CropR) override
    {
        std::lock_guard<std::recursive_mutex> lk(m_mtxProcessLock);
        if (m_u32CropR == u32CropR)
            return true;
        m_u32CropR = u32CropR;
        m_bNeedUpdateCropParam = true;
        return true;
    }

    uint32_t GetCropR() const override
    { return m_u32CropR; }

    bool SetCropB(uint32_t u32CropB) override
    {
        std::lock_guard<std::recursive_mutex> lk(m_mtxProcessLock);
        if (m_u32CropB == u32CropB)
            return true;
        m_u32CropB = u32CropB;
        m_bNeedUpdateCropParam = true;
        return true;
    }

    uint32_t GetCropB() const override
    { return m_u32CropB; }

    bool SetCropRatio(float fCropRatioL, float fCropRatioT, float fCropRatioR, float fCropRatioB) override
    {
        std::lock_guard<std::recursive_mutex> lk(m_mtxProcessLock);
        if (m_fCropRatioL == fCropRatioL && m_fCropRatioT == fCropRatioT && m_fCropRatioR == fCropRatioR && m_fCropRatioB == fCropRatioB)
            return true;
        m_fCropRatioL = fCropRatioL;
        m_fCropRatioT = fCropRatioT;
        m_fCropRatioR = fCropRatioR;
        m_fCropRatioB = fCropRatioB;
        m_bNeedUpdateCropRatioParam = true;
        return true;
    }

    bool SetCropRatioL(float fCropRatioL) override
    {
        std::lock_guard<std::recursive_mutex> lk(m_mtxProcessLock);
        if (m_fCropRatioL == fCropRatioL)
            return true;
        m_fCropRatioL = fCropRatioL;
        m_bNeedUpdateCropRatioParam = true;
        return true;
    }

    float GetCropRatioL() const override
    { return m_fCropRatioL; }

    bool SetCropRatioT(float fCropRatioT) override
    {
        std::lock_guard<std::recursive_mutex> lk(m_mtxProcessLock);
        if (m_fCropRatioT == fCropRatioT)
            return true;
        m_fCropRatioT = fCropRatioT;
        m_bNeedUpdateCropRatioParam = true;
        return true;
    }

    float GetCropRatioT() const override
    { return m_fCropRatioT; }

    bool SetCropRatioR(float fCropRatioR) override
    {
        std::lock_guard<std::recursive_mutex> lk(m_mtxProcessLock);
        if (m_fCropRatioR == fCropRatioR)
            return true;
        m_fCropRatioR = fCropRatioR;
        m_bNeedUpdateCropRatioParam = true;
        return true;
    }

    float GetCropRatioR() const override
    { return m_fCropRatioR; }

    bool SetCropRatioB(float fCropRatioB) override
    {
        std::lock_guard<std::recursive_mutex> lk(m_mtxProcessLock);
        if (m_fCropRatioB == fCropRatioB)
            return true;
        m_fCropRatioB = fCropRatioB;
        m_bNeedUpdateCropRatioParam = true;
        return true;
    }

    float GetCropRatioB() const override
    { return m_fCropRatioB; }

    // Scale
    bool SetScaleX(float fScaleX) override
    {
        std::lock_guard<std::recursive_mutex> lk(m_mtxProcessLock);
        if (m_fScaleX == fScaleX)
            return true;
        m_fScaleX = fScaleX;
        m_bNeedUpdateScaleParam = true;
        return true;
    }

    float GetScaleX() const override
    { return m_fScaleX; }

    bool SetScaleY(float fScaleY) override
    {
        std::lock_guard<std::recursive_mutex> lk(m_mtxProcessLock);
        if (m_fScaleY == fScaleY)
            return true;
        m_fScaleY = fScaleY;
        m_bNeedUpdateScaleParam = true;
        return true;
    }

    float GetScaleY() const override
    { return m_fScaleY; }

    void SetKeepAspectRatio(bool bEnable) override
    {
        std::lock_guard<std::recursive_mutex> lk(m_mtxProcessLock);
        if (m_bKeepAspectRatio != bEnable)
        {
            if (m_fScaleX != m_fScaleY)
                m_bNeedUpdateScaleParam = true;
            m_bKeepAspectRatio = bEnable;
        }
    }

    bool IsKeepAspectRatio() const override
    { return m_bKeepAspectRatio; }

    // Rotation
    bool SetRotation(float fAngle) override
    {
        std::lock_guard<std::recursive_mutex> lk(m_mtxProcessLock);
        int32_t n = (int32_t)trunc(fAngle/360);
        fAngle -= n*360;
        if (m_fRotateAngle == fAngle)
            return true;
        m_fRotateAngle = fAngle;
        m_bNeedUpdateRotateParam = true;
        return true;
    }

    float GetRotation() const override
    { return m_fRotateAngle; }

    // Opacity
    bool SetOpacity(float opacity) override
    {
        std::lock_guard<std::recursive_mutex> lk(m_mtxProcessLock);
        m_fOpacity = opacity;
        return true;
    }

    float GetOpacity() const override
    { return m_fOpacity; }

    imgui_json::value SaveAsJson() const override
    {
        imgui_json::value j;
        j["output_format"] = imgui_json::string(m_strOutputFormat);
        j["scale_type"] = imgui_json::number((int)m_eScaleType);
        j["pos_offset_curve"] = m_hPosOffsetCurve->SaveAsJson();
        j["pos_offset_keyframes_enabled"] = m_bEnablePosOffsetKeyFrames;
        return std::move(j);
    }

    bool LoadFromJson(const imgui_json::value& j) override
    {
        std::string strAttrName;
        strAttrName = "output_format";
        if (j.contains(strAttrName) && j[strAttrName].is_string())
        {
            if (!SetOutputFormat(j[strAttrName].get<imgui_json::string>()))
                return false;
        }
        strAttrName = "scale_type";
        if (j.contains(strAttrName) && j[strAttrName].is_number())
        {
            if (!SetScaleType((ScaleType)j[strAttrName].get<imgui_json::number>()))
                return false;
        }
        strAttrName = "pos_offset_curve";
        if (j.contains(strAttrName) && j[strAttrName].is_object())
        {
            m_hPosOffsetCurve->LoadFromJson(j[strAttrName]);
        }
        strAttrName = "pos_offset_keyframes_enabled";
        if (j.contains(strAttrName) && j[strAttrName].is_boolean())
            m_bEnablePosOffsetKeyFrames = j[strAttrName].get<imgui_json::boolean>();
        return true;
    }

    std::string GetError() const override
    { return m_strErrMsg; }

    bool SetKeyPoint(ImGui::KeyPointEditor &keypoint) override
    {
        std::lock_guard<std::recursive_mutex> lk(m_mtxProcessLock);
        m_keyPoints = keypoint;
        return true;
    }

    ImGui::KeyPointEditor* GetKeyPoint() override
    { return &m_keyPoints; }

protected:
    bool UpdateParamsByKeyFrames(int64_t i64Tick)
    {
        ImGui::ImNewCurve::KeyPoint::ValType tKpVal;
        tKpVal = m_hPosOffsetCurve->CalcPointVal((float)(m_bEnablePosOffsetKeyFrames ? i64Tick : m_tTimeRange.x), false, true);
        const auto i32PosOffX = (int32_t)round((float)m_u32InWidth*tKpVal.x);
        const auto i32PosOffY = (int32_t)round((float)m_u32InHeight*tKpVal.y);
        if (i32PosOffX != m_i32PosOffsetX || i32PosOffY != m_i32PosOffsetY)
        {
            m_i32PosOffsetX = i32PosOffX;
            m_i32PosOffsetY = i32PosOffY;
            m_fPosOffsetRatioX = tKpVal.x;
            m_fPosOffsetRatioY = tKpVal.y;
            m_bNeedUpdatePosOffsetParam = true;
        }

        return true;
    }

protected:
    std::recursive_mutex m_mtxProcessLock;
    uint32_t m_u32InWidth{0}, m_u32InHeight{0};
    uint32_t m_u32OutWidth{0}, m_u32OutHeight{0};
    std::string m_strOutputFormat;
    ScaleType m_eScaleType{SCALE_TYPE__FIT};
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
    bool m_bNeedUpdatePosOffsetRatioParam{false};
    bool m_bNeedUpdateCropParam{false};
    bool m_bNeedUpdateCropRatioParam{false};
    bool m_bNeedUpdateRotateParam{false};
    bool m_bNeedUpdateScaleParam{true};
    ImGui::ImNewCurve::Curve::Holder m_hPosOffsetCurve;
    bool m_bEnablePosOffsetKeyFrames{false};
    std::string m_strErrMsg;
    ImGui::KeyPointEditor m_keyPoints;
};
}