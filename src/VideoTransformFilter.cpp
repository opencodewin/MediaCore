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

#include "VideoTransformFilter.h"
#include "VideoTransformFilter_FFImpl.h"
#include <imconfig.h>
#if IMGUI_VULKAN_SHADER
#include "VideoTransformFilter_VulkanImpl.h"
#endif

using namespace std;

namespace MediaCore
{
    class VideoTransformFilter_Delegate : public VideoTransformFilter
    {
    public:
        VideoTransformFilter_Delegate()
        {
#if IMGUI_VULKAN_SHADER
            if (m_useVulkan)
                m_filter = new VideoTransformFilter_VulkanImpl();
            else
                m_filter = new VideoTransformFilter_FFImpl();
#else
            m_filter = new VideoTransformFilter_FFImpl();
#endif
        }

        ~VideoTransformFilter_Delegate()
        {
            delete m_filter;
        }

        const string GetFilterName() const override
        {
            return m_filter->GetFilterName();
        }

        Holder Clone(SharedSettings::Holder hSettings) override
        {
            VideoTransformFilter::Holder newInstance = VideoTransformFilter::CreateInstance();
            if (!newInstance->Initialize(hSettings))
                return nullptr;
            newInstance->SetScaleType(GetScaleType());
            newInstance->SetScaleH(GetScaleH());
            newInstance->SetScaleV(GetScaleV());
            newInstance->SetPositionOffsetH(GetPositionOffsetH());
            newInstance->SetPositionOffsetV(GetPositionOffsetV());
            newInstance->SetRotationAngle(GetRotationAngle());
            newInstance->SetCropMarginL(GetCropMarginL());
            newInstance->SetCropMarginT(GetCropMarginT());
            newInstance->SetCropMarginR(GetCropMarginR());
            newInstance->SetCropMarginB(GetCropMarginB());
            newInstance->SetPositionOffsetH(GetPositionOffsetHScale());
            newInstance->SetPositionOffsetV(GetPositionOffsetVScale());
            newInstance->SetCropMarginL(GetCropMarginLScale());
            newInstance->SetCropMarginT(GetCropMarginTScale());
            newInstance->SetCropMarginR(GetCropMarginRScale());
            newInstance->SetCropMarginB(GetCropMarginBScale());
            newInstance->SetKeyPoint(*GetKeyPoint());
            return newInstance;
        }

        ImGui::ImMat FilterImage(const ImGui::ImMat& vmat, int64_t pos) override
        {
            return m_filter->FilterImage(vmat, pos);
        }

        bool Initialize(SharedSettings::Holder hSettings) override
        {
            return m_filter->Initialize(hSettings);
        }

        bool SetOutputFormat(const string& outputFormat) override
        {
            return m_filter->SetOutputFormat(outputFormat);
        }

        bool SetScaleType(ScaleType type) override
        {
            return m_filter->SetScaleType(type);
        }

        bool SetPositionOffset(int32_t offsetH, int32_t offsetV) override
        {
            return m_filter->SetPositionOffset(offsetH, offsetV);
        }

        bool SetPositionOffsetH(int32_t value) override
        {
            return m_filter->SetPositionOffsetH(value);
        }

        bool SetPositionOffsetV(int32_t value) override
        {
            return m_filter->SetPositionOffsetV(value);
        }

        bool SetCropMargin(uint32_t left, uint32_t top, uint32_t right, uint32_t bottom) override
        {
            return m_filter->SetCropMargin(left, top, right, bottom);
        }

        bool SetCropMarginL(uint32_t value) override
        {
            return m_filter->SetCropMarginL(value);
        }

        bool SetCropMarginT(uint32_t value) override
        {
            return m_filter->SetCropMarginT(value);
        }

        bool SetCropMarginR(uint32_t value) override
        {
            return m_filter->SetCropMarginR(value);
        }

        bool SetCropMarginB(uint32_t value) override
        {
            return m_filter->SetCropMarginB(value);
        }

        bool SetRotationAngle(double angle) override
        {
            return m_filter->SetRotationAngle(angle);
        }

        bool SetScaleH(double scale) override
        {
            return m_filter->SetScaleH(scale);
        }

        bool SetScaleV(double scale) override
        {
            return m_filter->SetScaleV(scale);
        }

        bool SetKeyPoint(ImGui::KeyPointEditor &keypoint) override
        {
            return m_filter->SetKeyPoint(keypoint);
        }

        uint32_t GetInWidth() const override
        {
            return m_filter->GetInWidth();
        }

        uint32_t GetInHeight() const override
        {
            return m_filter->GetInHeight();
        }

        uint32_t GetOutWidth() const override
        {
            return m_filter->GetOutWidth();
        }

        uint32_t GetOutHeight() const override
        {
            return m_filter->GetOutHeight();
        }

        string GetOutputFormat() const override
        {
            return m_filter->GetOutputFormat();
        }

        ScaleType GetScaleType() const override
        {
            return m_filter->GetScaleType();
        }

        int32_t GetPositionOffsetH() const override
        {
            return m_filter->GetPositionOffsetH();
        }

        int32_t GetPositionOffsetV() const override
        {
            return m_filter->GetPositionOffsetV();
        }

        uint32_t GetCropMarginL() const override
        {
            return m_filter->GetCropMarginL();
        }

        uint32_t GetCropMarginT() const override
        {
            return m_filter->GetCropMarginT();
        }

        uint32_t GetCropMarginR() const override
        {
            return m_filter->GetCropMarginR();
        }

        uint32_t GetCropMarginB() const override
        {
            return m_filter->GetCropMarginB();
        }

        double GetRotationAngle() const override
        {
            return m_filter->GetRotationAngle();
        }

        double GetScaleH() const override
        {
            return m_filter->GetScaleH();
        }

        double GetScaleV() const override
        {
            return m_filter->GetScaleV();
        }

        ImGui::KeyPointEditor* GetKeyPoint() override
        {
            return m_filter->GetKeyPoint();
        }

        // new API for scaler value
        bool SetPositionOffset(float offsetH, float offsetV) override
        {
            return m_filter->SetPositionOffset(offsetH, offsetV);
        }
        
        bool SetPositionOffsetH(float value) override
        {
            return m_filter->SetPositionOffsetH(value);
        }

        bool SetPositionOffsetV(float value) override
        {
            return m_filter->SetPositionOffsetV(value);
        }

        bool SetCropMargin(float left, float top, float right, float bottom) override
        {
            return m_filter->SetCropMargin(left, top, right, bottom);
        }

        bool SetCropMarginL(float value) override
        {
            return m_filter->SetCropMarginL(value);
        }

        bool SetCropMarginT(float value) override
        {
            return m_filter->SetCropMarginT(value);
        }

        bool SetCropMarginR(float value) override
        {
            return m_filter->SetCropMarginR(value);
        }

        bool SetCropMarginB(float value) override
        {
            return m_filter->SetCropMarginB(value);
        }

        float GetPositionOffsetHScale() const override
        {
            return m_filter->GetPositionOffsetHScale();
        }

        float GetPositionOffsetVScale() const override
        {
            return m_filter->GetPositionOffsetVScale();
        }

        float GetCropMarginLScale() const override
        {
            return m_filter->GetCropMarginLScale();
        }

        float GetCropMarginTScale() const override
        {
            return m_filter->GetCropMarginTScale();
        }

        float GetCropMarginRScale() const override
        {
            return m_filter->GetCropMarginRScale();
        }

        float GetCropMarginBScale() const override
        {
            return m_filter->GetCropMarginBScale();
        }
        //
        
        string GetError() const override
        {
            return m_filter->GetError();
        }

    private:
        VideoTransformFilter_Base* m_filter;
#if IMGUI_VULKAN_SHADER
        bool m_useVulkan{true};
#endif
    };

    VideoTransformFilter::Holder VideoTransformFilter::CreateInstance()
    {
        return VideoTransformFilter::Holder(new VideoTransformFilter_Delegate());
    }
}
