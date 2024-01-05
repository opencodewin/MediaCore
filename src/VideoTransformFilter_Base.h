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
#include "VideoTransformFilter.h"
#include <mutex>

namespace MediaCore
{
    class VideoTransformFilter_Base : public VideoTransformFilter
    {
    public:
        virtual ~VideoTransformFilter_Base() {}

        uint32_t GetInWidth() const override
        { return m_inWidth; }

        uint32_t GetInHeight() const override
        { return m_inHeight; }

        uint32_t GetOutWidth() const override
        { return m_outWidth; }

        uint32_t GetOutHeight() const override
        { return m_outHeight; }

        std::string GetOutputFormat() const override
        { return m_outputFormat; }

        ScaleType GetScaleType() const override
        { return m_scaleType; }

        int32_t GetPositionOffsetH() const override
        { return m_posOffsetH; }

        int32_t GetPositionOffsetV() const override
        { return m_posOffsetV; }

        uint32_t GetCropMarginL() const override
        { return m_cropL; }

        uint32_t GetCropMarginT() const override
        { return m_cropT; }

        uint32_t GetCropMarginR() const override
        { return m_cropR; }

        uint32_t GetCropMarginB() const override
        { return m_cropB; }

        double GetRotationAngle() const override
        { return m_rotateAngle; }

        double GetScaleH() const override
        { return m_scaleRatioH; }

        double GetScaleV() const override
        { return m_scaleRatioV; }

        ImGui::KeyPointEditor* GetKeyPoint() override
        { return &m_keyPoints; }

        bool SetScaleType(ScaleType type) override
        {
            std::lock_guard<std::recursive_mutex> lk(m_processLock);
            if (type < SCALE_TYPE__FIT || type > SCALE_TYPE__STRETCH)
            {
                m_errMsg = "INVALID argument 'type'!";
                return false;
            }
            if (m_scaleType == type)
                return true;
            m_scaleType = type;
            m_needUpdateScaleParam = true;
            return true;
        }

        bool SetPositionOffset(int32_t offsetH, int32_t offsetV) override
        {
            std::lock_guard<std::recursive_mutex> lk(m_processLock);
            if (m_posOffsetH == offsetH && m_posOffsetV == offsetV)
                return true;
            m_posOffsetH = offsetH;
            m_posOffsetV = offsetV;
            m_needUpdatePositionParam = true;
            return true;
        }

        bool SetPositionOffsetH(int32_t value) override
        {
            std::lock_guard<std::recursive_mutex> lk(m_processLock);
            if (m_posOffsetH == value)
                return true;
            m_posOffsetH = value;
            m_needUpdatePositionParam = true;
            return true;
        }

        bool SetPositionOffsetV(int32_t value) override
        {
            std::lock_guard<std::recursive_mutex> lk(m_processLock);
            if (m_posOffsetV == value)
                return true;
            m_posOffsetV = value;
            m_needUpdatePositionParam = true;
            return true;
        }

        bool SetCropMargin(uint32_t left, uint32_t top, uint32_t right, uint32_t bottom) override
        {
            std::lock_guard<std::recursive_mutex> lk(m_processLock);
            if (m_cropL == left && m_cropT == top && m_cropR == right && m_cropB == bottom)
                return true;
            m_cropL = left;
            m_cropT = top;
            m_cropR = right;
            m_cropB = bottom;
            m_needUpdateCropParam = true;
            return true;
        }

        bool SetCropMarginL(uint32_t value) override
        {
            std::lock_guard<std::recursive_mutex> lk(m_processLock);
            if (m_cropL == value)
                return true;
            m_cropL = value;
            m_needUpdateCropParam = true;
            return true;
        }

        bool SetCropMarginT(uint32_t value) override
        {
            std::lock_guard<std::recursive_mutex> lk(m_processLock);
            if (m_cropT == value)
                return true;
            m_cropT = value;
            m_needUpdateCropParam = true;
            return true;
        }

        bool SetCropMarginR(uint32_t value) override
        {
            std::lock_guard<std::recursive_mutex> lk(m_processLock);
            if (m_cropR == value)
                return true;
            m_cropR = value;
            m_needUpdateCropParam = true;
            return true;
        }

        bool SetCropMarginB(uint32_t value) override
        {
            std::lock_guard<std::recursive_mutex> lk(m_processLock);
            if (m_cropB == value)
                return true;
            m_cropB = value;
            m_needUpdateCropParam = true;
            return true;
        }

        bool SetRotationAngle(double angle) override
        {
            std::lock_guard<std::recursive_mutex> lk(m_processLock);
            int32_t n = (int32_t)trunc(angle/360);
            angle -= n*360;
            if (m_rotateAngle == angle)
                return true;
            m_rotateAngle = angle;
            m_needUpdateRotateParam = true;
            return true;
        }

        bool SetScaleH(double scale) override
        {
            std::lock_guard<std::recursive_mutex> lk(m_processLock);
            if (m_scaleRatioH == scale)
                return true;
            m_scaleRatioH = scale;
            m_needUpdateScaleParam = true;
            return true;
        }

        bool SetScaleV(double scale) override
        {
            std::lock_guard<std::recursive_mutex> lk(m_processLock);
            if (m_scaleRatioV == scale)
                return true;
            m_scaleRatioV = scale;
            m_needUpdateScaleParam = true;
            return true;
        }

        bool SetKeyPoint(ImGui::KeyPointEditor &keypoint) override
        {
            std::lock_guard<std::recursive_mutex> lk(m_processLock);
            m_keyPoints = keypoint;
            return true;
        }

        // new API
        bool SetPositionOffset(float offsetH, float offsetV) override
        {
            std::lock_guard<std::recursive_mutex> lk(m_processLock);
            if (m_fposOffsetH == offsetH && m_fposOffsetV == offsetV)
                return true;
            m_fposOffsetH = offsetH;
            m_fposOffsetV = offsetV;
            m_needUpdatePositionParamScale = true;
            return true;
        }

        bool SetPositionOffsetH(float value) override
        {
            std::lock_guard<std::recursive_mutex> lk(m_processLock);
            if (m_fposOffsetH == value)
                return true;
            m_fposOffsetH = value;
            m_needUpdatePositionParamScale = true;
            return true;
        }

        bool SetPositionOffsetV(float value) override
        {
            std::lock_guard<std::recursive_mutex> lk(m_processLock);
            if (m_fposOffsetV == value)
                return true;
            m_fposOffsetV = value;
            m_needUpdatePositionParamScale = true;
            return true;
        }

        bool SetCropMargin(float left, float top, float right, float bottom) override
        {
            std::lock_guard<std::recursive_mutex> lk(m_processLock);
            if (m_fcropL == left && m_fcropT == top && m_fcropR == right && m_fcropB == bottom)
                return true;
            m_fcropL = left;
            m_fcropT = top;
            m_fcropR = right;
            m_fcropB = bottom;
            m_needUpdateCropParamScale = true;
            return true;
        }

        bool SetCropMarginL(float value) override
        {
            std::lock_guard<std::recursive_mutex> lk(m_processLock);
            if (m_fcropL == value)
                return true;
            m_fcropL = value;
            m_needUpdateCropParamScale = true;
            return true;
        }

        bool SetCropMarginT(float value) override
        {
            std::lock_guard<std::recursive_mutex> lk(m_processLock);
            if (m_fcropT == value)
                return true;
            m_fcropT = value;
            m_needUpdateCropParamScale = true;
            return true;
        }

        bool SetCropMarginR(float value) override
        {
            std::lock_guard<std::recursive_mutex> lk(m_processLock);
            if (m_fcropR == value)
                return true;
            m_fcropR = value;
            m_needUpdateCropParamScale = true;
            return true;
        }

        bool SetCropMarginB(float value) override
        {
            std::lock_guard<std::recursive_mutex> lk(m_processLock);
            if (m_fcropB == value)
                return true;
            m_fcropB = value;
            m_needUpdateCropParamScale = true;
            return true;
        }

        float GetPositionOffsetHScale() const override
        { return m_fposOffsetH; }

        float GetPositionOffsetVScale() const override
        { return m_fposOffsetV; }

        float GetCropMarginLScale() const override
        { return m_fcropL; }

        float GetCropMarginTScale() const override
        { return m_fcropT; }

        float GetCropMarginRScale() const override
        { return m_fcropR; }

        float GetCropMarginBScale() const override
        { return m_fcropB; }

        std::string GetError() const override
        { return m_errMsg; }

    protected:
        uint32_t m_inWidth{0}, m_inHeight{0};
        uint32_t m_outWidth{0}, m_outHeight{0};
        std::string m_outputFormat;
        ScaleType m_scaleType{SCALE_TYPE__FIT};
        uint32_t m_cropL{0}, m_cropR{0}, m_cropT{0}, m_cropB{0};
        uint32_t m_cropRectX{0}, m_cropRectY{0}, m_cropRectW{0}, m_cropRectH{0};
        double m_scaleRatioH{1}, m_scaleRatioV{1};
        double m_rotateAngle{0};
        int32_t m_posOffsetH{0}, m_posOffsetV{0};
        ImGui::KeyPointEditor m_keyPoints;
        std::recursive_mutex m_processLock;
        bool m_needUpdateScaleParam{true};
        bool m_needUpdatePositionParam{false};
        bool m_needUpdateCropParam{false};
        bool m_needUpdateCropParamScale{false};
        bool m_needUpdatePositionParamScale{false};
        bool m_needUpdateRotateParam{false};
        float m_fcropL{0}, m_fcropR{0}, m_fcropT{0}, m_fcropB{0};
        float m_fposOffsetH{0}, m_fposOffsetV{0};
        std::string m_errMsg;
    };
}