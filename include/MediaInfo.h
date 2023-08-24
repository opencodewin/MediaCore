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
#include <cstdint>
#include <string>
#include <memory>
#include <vector>
#include <ostream>
#include "MediaCore.h"

namespace MediaCore
{
    enum class MediaType
    {
        UNKNOWN = 0,
        VIDEO,
        AUDIO,
        SUBTITLE,
    };

    struct Ratio
    {
        Ratio() {}
        Ratio(int32_t _num, int32_t _den) : num(_num), den(_den) {}
        Ratio(const std::string& ratstr);

        int32_t num{0};
        int32_t den{0};

        static inline bool IsValid(const Ratio& r)
        { return r.num != 0 && r.den != 0; }

        bool operator==(const Ratio& r)
        { return num*r.den == den*r.num; }
        bool operator!=(const Ratio& r)
        { return !(*this == r); }

        friend std::ostream& operator<<(std::ostream& os, const Ratio& r);
    };

    struct Value
    {
        enum Type
        {
            VT_INT = 0,
            VT_DOUBLE,
            VT_BOOL,
            VT_STRING,
            VT_FLAGS,
            VT_RATIO,
        };

        Value() = default;
        Value(const Value&) = default;
        Value(Value&&) = default;
        Value& operator=(const Value&) = default;
        Value(int64_t val) : type(VT_INT) { numval.i64=val; }
        Value(uint64_t val) : type(VT_INT) { numval.i64=val; }
        Value(int32_t val) : type(VT_INT) { numval.i64=val; }
        Value(uint32_t val) : type(VT_INT) { numval.i64=val; }
        Value(int16_t val) : type(VT_INT) { numval.i64=val; }
        Value(uint16_t val) : type(VT_INT) { numval.i64=val; }
        Value(int8_t val) : type(VT_INT) { numval.i64=val; }
        Value(uint8_t val) : type(VT_INT) { numval.i64=val; }
        Value(double val) : type(VT_DOUBLE) { numval.dbl=val; }
        Value(float val) : type(VT_DOUBLE) { numval.dbl=val; }
        Value(bool val) : type(VT_BOOL) { numval.bln=val; }
        Value(const char* val) : type(VT_STRING) { strval=std::string(val); }
        Value(const std::string& val) : type(VT_STRING) { strval=val; }
        Value(const Ratio& rat) : type(VT_RATIO) { ratval=rat; }

        template <typename T>
        Value(Type _type, const T& val) : type(_type)
        {
            switch (_type)
            {
                case VT_INT:    numval.i64 = static_cast<int64_t>(val); break;
                case VT_DOUBLE: numval.dbl = static_cast<double>(val); break;
                case VT_BOOL:   numval.bln = static_cast<bool>(val); break;
                case VT_STRING: strval = std::string(val); break;
                case VT_FLAGS:  numval.i64 = static_cast<int64_t>(val); break;
                case VT_RATIO:  ratval = Ratio(val); break;
            }
        }

        Type type;
        union
        {
            int64_t i64;
            double dbl;
            bool bln;
        } numval;
        std::string strval;
        Ratio ratval;

        friend std::ostream& operator<<(std::ostream& os, const Value& val);
    };

    struct Stream
    {
        using Holder = std::shared_ptr<Stream>;
        virtual ~Stream() {}

        MediaType type{MediaType::UNKNOWN};
        uint64_t bitRate{0};
        double startTime;
        double duration;
        Ratio timebase;
    };

    struct VideoStream : public Stream
    {
        VideoStream() { type = MediaType::VIDEO; }
        uint32_t width{0};
        uint32_t height{0};
        std::string format;
        std::string codec;
        Ratio sampleAspectRatio;
        Ratio avgFrameRate;
        Ratio realFrameRate;
        uint64_t frameNum{0};
        bool isImage{false};
        bool isHdr{false};
        uint8_t bitDepth{0};
    };

    struct AudioStream : public Stream
    {
        AudioStream() { type = MediaType::AUDIO; }
        uint32_t channels{0};
        uint32_t sampleRate{0};
        std::string format;
        std::string codec;
        uint8_t bitDepth{0};
    };

    struct SubtitleStream : public Stream
    {
        SubtitleStream() { type = MediaType::SUBTITLE; }
    };

    struct MediaInfo
    {
        using Holder = std::shared_ptr<MediaInfo>;
        virtual ~MediaInfo() {}

        std::string url;
        std::vector<Stream::Holder> streams;
        double startTime{0};
        double duration{-1};
        bool isComplete{true};
    };
}
