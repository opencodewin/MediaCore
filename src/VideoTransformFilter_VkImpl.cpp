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

#include <imconfig.h>
#if IMGUI_VULKAN_SHADER
#include <cmath>
#include <imgui.h>
#include <warpAffine_vulkan.h>
#include "VideoTransformFilter_Base.h"
#include "Logger.h"

using namespace std;
using namespace Logger;

namespace MediaCore
{
class VideoTransformFilter_VkImpl : public VideoTransformFilter_Base
{
public:
    ~VideoTransformFilter_VkImpl()
    {
        if (m_pWarpAffine)
        {
            delete m_pWarpAffine;
            m_pWarpAffine = nullptr;
        }
    }

    const std::string GetFilterName() const
    {
        return "VideoTransformFilter_VkImpl";
    }

    bool Initialize(SharedSettings::Holder hSettings)
    {
        const auto u32OutWidth = hSettings->VideoOutWidth();
        const auto u32OutHeight = hSettings->VideoOutHeight();
        if (u32OutWidth == 0 || u32OutHeight == 0)
        {
            m_strErrMsg = "INVALID argument! 'VideoOutWidth' and 'VideoOutHeight' must be positive value.";
            return false;
        }
        m_u32OutWidth = u32OutWidth;
        m_u32OutHeight = u32OutHeight;
        m_mAffineMatrix.create_type(3, 2, IM_DT_FLOAT32);
        memset(m_mAffineMatrix.data, 0, m_mAffineMatrix.elemsize*m_mAffineMatrix.total());
        m_mAffineMatrix.at<float>(0, 0) = 1;
        m_mAffineMatrix.at<float>(1, 1) = 1;
        if (!SetOutputFormat("rgba"))
            return false;
        m_bNeedUpdateScaleParam = true;
        return true;
    }


    bool SetOutputFormat(const string& outputFormat)
    {
        lock_guard<recursive_mutex> lk(m_mtxProcessLock);
        if (outputFormat != "rgba")
        {
            m_strErrMsg = "ONLY support using 'rgba' as output format!";
            return false;
        }

        m_strOutputFormat = outputFormat;
        return true;
    }

    ImGui::ImMat FilterImage(const ImGui::ImMat& vmat, int64_t pos)
    {
        ImGui::ImMat res;
        if (!_filterImage(vmat, res, pos))
        {
            res.release();
            Log(Error) << "VideoTransformFilter_VkImpl::FilterImage() FAILED! " << m_strErrMsg << endl;
        }
        return res;
    }

private:
    bool _filterImage(const ImGui::ImMat& inMat, ImGui::ImMat& outMat, int64_t pos)
    {
        m_u32InWidth = inMat.w; m_u32InHeight = inMat.h;
        if (!UpdateParamsByKeyFrames(pos))
        {
            Log(Error) << "[VideoTransformFilter_VkImpl::_filterImage] 'UpdateParamsByKeyFrames()' at pos " << pos << " FAILED!" << endl;
            return false;
        }

        if (m_bNeedUpdateScaleParam)
        {
            uint32_t u32FitScaleWidth{m_u32InWidth}, u32FitScaleHeight{m_u32InHeight};
            switch (m_eAspectFitType)
            {
                case ASPECT_FIT_TYPE__FIT:
                if (m_u32InWidth*m_u32OutHeight > m_u32InHeight*m_u32OutWidth)
                {
                    u32FitScaleWidth = m_u32OutWidth;
                    u32FitScaleHeight = (uint32_t)round((float)m_u32InHeight*m_u32OutWidth/m_u32InWidth);
                }
                else
                {
                    u32FitScaleHeight = m_u32OutHeight;
                    u32FitScaleWidth = (uint32_t)round((float)m_u32InWidth*m_u32OutHeight/m_u32InHeight);
                }
                break;
                case ASPECT_FIT_TYPE__CROP:
                u32FitScaleWidth = m_u32InWidth;
                u32FitScaleHeight = m_u32InHeight;
                break;
                case ASPECT_FIT_TYPE__FILL:
                if (m_u32InWidth*m_u32OutHeight > m_u32InHeight*m_u32OutWidth)
                {
                    u32FitScaleHeight = m_u32OutHeight;
                    u32FitScaleWidth = (uint32_t)round((float)m_u32InWidth*m_u32OutHeight/m_u32InHeight);
                }
                else
                {
                    u32FitScaleWidth = m_u32OutWidth;
                    u32FitScaleHeight = (uint32_t)round((float)m_u32InHeight*m_u32OutWidth/m_u32InWidth);
                }
                break;
                case ASPECT_FIT_TYPE__STRETCH:
                u32FitScaleWidth = m_u32OutWidth;
                u32FitScaleHeight = m_u32OutHeight;
                break;
            }
            m_fRealScaleRatioX = (float)u32FitScaleWidth/m_u32InWidth*m_fScaleX;
            m_fRealScaleRatioY = (float)u32FitScaleHeight/m_u32InHeight*(m_bKeepAspectRatio ? m_fScaleX : m_fScaleY);
        }
        if (m_bNeedUpdateScaleParam || m_bNeedUpdateRotationParam || m_bNeedUpdatePosOffsetParam)
        {
            float _x_scale = 1.f / (m_fRealScaleRatioX + FLT_EPSILON);
            float _y_scale = 1.f / (m_fRealScaleRatioY + FLT_EPSILON);
            float _angle = m_fRotateAngle / 180.f * M_PI;
            float alpha_00 = cos(_angle) * _x_scale;
            float alpha_11 = cos(_angle) * _y_scale;
            float beta_01 = sin(_angle) * _x_scale;
            float beta_10 = sin(_angle) * _y_scale;
            float x_diff = (float)m_u32OutWidth - (float)m_u32InWidth;
            float y_diff = (float)m_u32OutHeight - (float)m_u32InHeight;
            float _x_diff = (m_u32OutWidth + m_u32InWidth * m_fRealScaleRatioX) / 2.f;
            float _y_diff = (m_u32OutHeight + m_u32InHeight * m_fRealScaleRatioY) / 2.f;
            float x_offset = (float)m_i32PosOffsetX / (float)m_u32OutWidth;
            float _x_offset = x_offset * _x_diff + x_diff / 2;
            float y_offset = (float)m_i32PosOffsetY / (float)m_u32OutHeight;
            float _y_offset = y_offset * _y_diff + y_diff / 2;
            int center_x = m_u32InWidth / 2.f + _x_offset;
            int center_y = m_u32InHeight / 2.f + _y_offset;
            m_mAffineMatrix.at<float>(0, 0) =  alpha_00;
            m_mAffineMatrix.at<float>(1, 0) = beta_01;
            m_mAffineMatrix.at<float>(2, 0) = (1 - alpha_00) * center_x - beta_01 * center_y - _x_offset;
            m_mAffineMatrix.at<float>(0, 1) = -beta_10;
            m_mAffineMatrix.at<float>(1, 1) = alpha_11;
            m_mAffineMatrix.at<float>(2, 1) = beta_10 * center_x + (1 - alpha_11) * center_y - _y_offset;
            m_bNeedUpdateScaleParam = m_bNeedUpdateRotationParam = m_bNeedUpdatePosOffsetParam = false;
            UpdatePassThrough();
        }

        if (m_bNeedUpdateCropRatioParam)
        {
            m_u32CropL = (uint32_t)((float)m_u32InWidth*m_fCropRatioL);
            m_u32CropR = (uint32_t)((float)m_u32InWidth*m_fCropRatioR);
            m_u32CropT = (uint32_t)((float)m_u32InHeight*m_fCropRatioT);
            m_u32CropB = (uint32_t)((float)m_u32InHeight*m_fCropRatioB);
            m_bNeedUpdateCropRatioParam = false;
            m_bNeedUpdateCropParam = true;
        }
        if (m_bNeedUpdateCropParam)
        {
            auto l = m_u32CropL;
            auto t = m_u32CropT;
            auto r = m_u32CropR;
            auto b = m_u32CropB;
            if (l+r > m_u32InWidth)
            {
                const auto tmp = l;
                l = m_u32InWidth-r;
                r = m_u32InWidth-tmp;
            }
            if (t+b > m_u32InHeight)
            {
                const auto tmp = t;
                t = m_u32InHeight-b;
                b = m_u32InHeight-tmp;
            }
            m_tCropRect = ImPixel((float)l, (float)t, (float)r, (float)b);
            m_bNeedUpdateCropParam = false;
            UpdatePassThrough();
        }

        if (m_bPassThrough)
            outMat = inMat;
        else
        {
            ImGui::VkMat vkmat; vkmat.type = inMat.type;
            vkmat.w = m_u32OutWidth; vkmat.h = m_u32OutHeight;
            if (!m_pWarpAffine)
                m_pWarpAffine = new ImGui::warpAffine_vulkan();
            m_pWarpAffine->warp(inMat, vkmat, m_mAffineMatrix, m_eInterpMode, ImPixel(0, 0, 0, 0), m_tCropRect);
            vkmat.time_stamp = inMat.time_stamp;
            vkmat.rate = inMat.rate;
            vkmat.flags = inMat.flags;
            outMat = vkmat;
        }
        return true;
    }

    void UpdatePassThrough()
    {
        if (m_fRealScaleRatioX == 1 && m_fRealScaleRatioY == 1 &&
            m_u32InWidth == m_u32OutWidth && m_u32InHeight == m_u32OutHeight &&
            m_u32CropL == 0 && m_u32CropT == 0 && m_u32CropR == 0 && m_u32CropB == 0 &&
            m_fRotateAngle == 0 &&
            m_i32PosOffsetX == 0 && m_i32PosOffsetY == 0)
            m_bPassThrough = true;
        else
            m_bPassThrough = false;
    }

private:
    ImGui::warpAffine_vulkan* m_pWarpAffine{nullptr};
    ImGui::ImMat m_mAffineMatrix;
    float m_fRealScaleRatioX{1}, m_fRealScaleRatioY{1};
    ImPixel m_tCropRect;
    ImInterpolateMode m_eInterpMode{IM_INTERPOLATE_BICUBIC};
    bool m_bPassThrough{false};
    std::string m_strErrMsg;
};

VideoTransformFilter::Holder CreateVideoTransformFilterInstance_VkImpl()
{
    return VideoTransformFilter::Holder(new VideoTransformFilter_VkImpl(), [] (auto p) {
        VideoTransformFilter_VkImpl* ptr = dynamic_cast<VideoTransformFilter_VkImpl*>(p);
        delete ptr;
    });
}
}
#endif
