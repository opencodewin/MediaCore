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
#include <string>
#include <memory>
#include <MatUtilsVecTypeDef.h>
#include <ImNewCurve.h>
#include <imgui_curve.h>
#include <imgui_json.h>
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

        virtual const std::string GetFilterName() const = 0;
        virtual uint32_t GetInWidth() const = 0;
        virtual uint32_t GetInHeight() const = 0;
        virtual uint32_t GetOutWidth() const = 0;
        virtual uint32_t GetOutHeight() const = 0;

        virtual bool SetOutputFormat(const std::string& outputFormat) = 0;
        virtual std::string GetOutputFormat() const = 0;
        virtual bool SetScaleType(ScaleType type) = 0;
        virtual ScaleType GetScaleType() const = 0;
        virtual bool SetTimeRange(const MatUtils::Vec2<int64_t>& tTimeRange) = 0;
        virtual MatUtils::Vec2<int64_t> GetTimeRange() const = 0;

        virtual ImGui::ImMat FilterImage(const ImGui::ImMat& vmat, int64_t pos) = 0;
        virtual VideoFrame::Holder FilterImage(VideoFrame::Holder hVfrm, int64_t pos) = 0;

        // Position
        virtual bool SetPosOffset(int32_t i32PosOffX, int32_t i32PosOffY) = 0;
        virtual bool SetPosOffsetX(int32_t i32PosOffX) = 0;
        virtual int32_t GetPosOffsetX() const = 0;
        virtual bool SetPosOffsetY(int32_t i32PosOffY) = 0;
        virtual int32_t GetPosOffsetY() const = 0;
        virtual bool SetPosOffsetRatio(float fPosOffRatioX, float fPosOffRatioY) = 0;
        virtual bool SetPosOffsetRatioX(float fPosOffRatioX) = 0;
        virtual float GetPosOffsetRatioX() const = 0;
        virtual bool SetPosOffsetRatioY(float fPosOffRatioY) = 0;
        virtual float GetPosOffsetRatioY() const = 0;
        virtual bool SetPosOffsetRatio(int64_t i64Tick, float fPosOffRatioX, float fPosOffRatioY) = 0;
        virtual bool SetPosOffsetRatioX(int64_t i64Tick, float fPosOffRatioX) = 0;
        virtual float GetPosOffsetRatioX(int64_t i64Tick) const = 0;
        virtual bool SetPosOffsetRatioY(int64_t i64Tick, float fPosOffRatioY) = 0;
        virtual float GetPosOffsetRatioY(int64_t i64Tick) const = 0;
        virtual void EnablePosOffsetKeyFrames(bool bEnable) = 0;
        virtual bool IsPosOffsetKeyFramesEnabled() const = 0;
        // Crop
        virtual bool SetCrop(uint32_t u32CropL, uint32_t u32CropT, uint32_t u32CropR, uint32_t u32CropB) = 0;
        virtual bool SetCropL(uint32_t u32CropL) = 0;
        virtual uint32_t GetCropL() const = 0;
        virtual bool SetCropT(uint32_t u32CropT) = 0;
        virtual uint32_t GetCropT() const = 0;
        virtual bool SetCropR(uint32_t u32CropR) = 0;
        virtual uint32_t GetCropR() const = 0;
        virtual bool SetCropB(uint32_t u32CropB) = 0;
        virtual uint32_t GetCropB() const = 0;
        virtual bool SetCropRatio(float fCropRatioL, float fCropRatioT, float fCropRatioR, float fCropRatioB) = 0;
        virtual bool SetCropRatioL(float fCropRatioL) = 0;
        virtual float GetCropRatioL() const = 0;
        virtual bool SetCropRatioT(float fCropRatioT) = 0;
        virtual float GetCropRatioT() const = 0;
        virtual bool SetCropRatioR(float fCropRatioR) = 0;
        virtual float GetCropRatioR() const = 0;
        virtual bool SetCropRatioB(float fCropRatioB) = 0;
        virtual float GetCropRatioB() const = 0;
        // Scale
        virtual bool SetScaleX(float fScaleX) = 0;
        virtual float GetScaleX() const = 0;
        virtual bool SetScaleY(float fScaleY) = 0;
        virtual float GetScaleY() const = 0;
        virtual void SetKeepAspectRatio(bool bEnable) = 0;
        virtual bool IsKeepAspectRatio() const = 0;
        // Rotation
        virtual bool SetRotation(float fAngle) = 0;
        virtual float GetRotation() const = 0;
        // Opacity
        virtual bool SetOpacity(float fOpacity) = 0;
        virtual float GetOpacity() const = 0;

        virtual imgui_json::value SaveAsJson() const = 0;
        virtual bool LoadFromJson(const imgui_json::value& j) = 0;
        virtual std::string GetError() const = 0;

        virtual bool SetKeyPoint(ImGui::KeyPointEditor &keypoint) = 0;
        virtual ImGui::KeyPointEditor* GetKeyPoint() = 0;
    };
}