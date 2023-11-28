/*
    Copyright (c) 2023 CodeWin

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
#include <string>
#include <memory>
#include "imgui_extra_widget.h"
#include "imgui_curve.h"
#include "SharedSettings.h"

namespace MediaCore
{
    enum ScaleType
    {
        SCALE_TYPE__FIT = 0,
        SCALE_TYPE__CROP,
        SCALE_TYPE__FILL,
        SCALE_TYPE__STRETCH,
    };

    struct VideoTransformFilter
    {
        using Holder = std::shared_ptr<VideoTransformFilter>;
        static Holder CreateInstance();

        virtual bool Initialize(SharedSettings::Holder hSettings) = 0;
        virtual Holder Clone(SharedSettings::Holder hSettings) = 0;
        virtual bool SetOutputFormat(const std::string& outputFormat) = 0;
        virtual bool SetScaleType(ScaleType type) = 0;
        virtual bool SetPositionOffset(int32_t offsetH, int32_t offsetV) = 0;
        virtual bool SetPositionOffsetH(int32_t value) = 0;
        virtual bool SetPositionOffsetV(int32_t value) = 0;
        virtual bool SetCropMargin(uint32_t left, uint32_t top, uint32_t right, uint32_t bottom) = 0;
        virtual bool SetCropMarginL(uint32_t value) = 0;
        virtual bool SetCropMarginT(uint32_t value) = 0;
        virtual bool SetCropMarginR(uint32_t value) = 0;
        virtual bool SetCropMarginB(uint32_t value) = 0;
        virtual bool SetRotationAngle(double angle) = 0;
        virtual bool SetScaleH(double scale) = 0;
        virtual bool SetScaleV(double scale) = 0;
        virtual bool SetKeyPoint(ImGui::KeyPointEditor &keypoint) = 0;
        virtual ImGui::ImMat FilterImage(const ImGui::ImMat& vmat, int64_t pos) = 0;

        virtual const std::string GetFilterName() const = 0;
        virtual std::string GetOutputFormat() const = 0;
        virtual uint32_t GetInWidth() const = 0;
        virtual uint32_t GetInHeight() const = 0;
        virtual uint32_t GetOutWidth() const = 0;
        virtual uint32_t GetOutHeight() const = 0;
        virtual ScaleType GetScaleType() const = 0;
        virtual int32_t GetPositionOffsetH() const = 0;
        virtual int32_t GetPositionOffsetV() const = 0;
        virtual uint32_t GetCropMarginL() const = 0;
        virtual uint32_t GetCropMarginT() const = 0;
        virtual uint32_t GetCropMarginR() const = 0;
        virtual uint32_t GetCropMarginB() const = 0;
        virtual double GetRotationAngle() const = 0;
        virtual double GetScaleH() const = 0;
        virtual double GetScaleV() const = 0;
        virtual ImGui::KeyPointEditor* GetKeyPoint() = 0;

        // new API for scaler value
        virtual bool SetPositionOffset(float offsetH, float offsetV) = 0;
        virtual bool SetPositionOffsetH(float value) = 0;
        virtual bool SetPositionOffsetV(float value) = 0;
        virtual bool SetCropMargin(float left, float top, float right, float bottom) = 0;
        virtual bool SetCropMarginL(float value) = 0;
        virtual bool SetCropMarginT(float value) = 0;
        virtual bool SetCropMarginR(float value) = 0;
        virtual bool SetCropMarginB(float value) = 0;
        virtual float GetPositionOffsetHScale() const = 0;
        virtual float GetPositionOffsetVScale() const = 0;
        virtual float GetCropMarginLScale() const = 0;
        virtual float GetCropMarginTScale() const = 0;
        virtual float GetCropMarginRScale() const = 0;
        virtual float GetCropMarginBScale() const = 0;
        // 

        virtual std::string GetError() const = 0;
    };
}