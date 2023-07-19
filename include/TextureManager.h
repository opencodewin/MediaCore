#pragma once

#include <cstdint>
#include <memory>
#include <utility>
#include <string>
#include <thread>
#include <ostream>
#include "imgui.h"
#include "immat.h"
#include "MediaCore.h"
#include "Logger.h"

namespace RenderUtils
{
template<typename T>
struct Vec2 : public std::pair<T,T>
{
    using elemtype = T;
    using basetype = std::pair<elemtype,elemtype>;
    Vec2() : x(basetype::first), y(basetype::second) {}
    Vec2(const elemtype& a, const elemtype& b) : basetype(a, b), x(basetype::first), y(basetype::second) {}
    Vec2(const basetype& vec2) : basetype(vec2), x(basetype::first), y(basetype::second) {}
    Vec2(const Vec2<T>& vec2) : basetype(vec2.x, vec2.y), x(basetype::first), y(basetype::second) {}
    template<typename U>
    Vec2(const Vec2<U>& vec2) : basetype(vec2.x, vec2.y), x(basetype::first), y(basetype::second) {}
    Vec2<T>& operator=(const Vec2<T>& a) { *static_cast<basetype*>(this) = static_cast<const basetype>(a); return *this; }
    Vec2<T> operator+(const Vec2<T>& a) const { return {x+a.x, y+a.y}; }
    Vec2<T> operator-(const Vec2<T>& a) const { return {x-a.x, y-a.y}; }
    Vec2<T> operator*(const Vec2<T>& a) const { return {x*a.x, y*a.y}; }
    Vec2<T> operator/(const Vec2<T>& a) const { return {x/a.x, y/a.y}; }
    operator ImVec2() const { return ImVec2(x, y); }

    elemtype& x;
    elemtype& y;
};

template <typename T>
struct Rect : public std::pair<Vec2<T>,Vec2<T>>
{
    using elemtype = Vec2<T>;
    using basetype = std::pair<elemtype,elemtype>;
    Rect() : lt(basetype::first), rb(basetype::second) {}
    Rect(const elemtype& a, const elemtype& b) : basetype(a, b), lt(basetype::first), rb(basetype::second) {}
    Rect(const basetype& rect) : basetype(rect), lt(basetype::first), rb(basetype::second) {}
    Rect(const Rect<T>& rect) : basetype(rect.lt, rect.rb), lt(basetype::first), rb(basetype::second) {}
    template<typename U>
    Rect(const Rect<U>& rect) : basetype(rect.lt, rect.rb), lt(basetype::first), rb(basetype::second) {}
    Rect<T>& operator=(const Rect<T>& a) { *static_cast<basetype*>(this) = static_cast<const basetype>(a); return *this; }
    Vec2<T> Size() const { return {rb.x-lt.x, rb.y-lt.y}; }
    Rect<T> operator/(const Vec2<T>& a) const { return {lt/a, rb/a}; }

    elemtype& lt;   // left top
    elemtype& rb;   // right bottom
};

struct ManagedTexture
{
    using Holder = std::shared_ptr<ManagedTexture>;

    virtual ImTextureID TextureID() const = 0;
    virtual Rect<float> GetDisplayRoi() const = 0;
    virtual bool IsValid() const = 0;
    virtual void Invalidate() = 0;
    virtual bool RenderMatToTexture(const ImGui::ImMat& vmat) = 0;

    virtual std::string GetError() const = 0;
};

struct TextureManager
{
    using Holder = std::shared_ptr<TextureManager>;
    static MEDIACORE_API Holder CreateInstance();
    static MEDIACORE_API Holder GetDefaultInstance();
    static MEDIACORE_API void ReleaseDefaultInstance();

    virtual ManagedTexture::Holder CreateManagedTextureFromMat(const ImGui::ImMat& vmat, Vec2<int32_t>& textureSize, ImDataType dataType = IM_DT_INT8) = 0;
    virtual bool CreateTexturePool(const std::string& name, const Vec2<int32_t>& textureSize, ImDataType dataType, uint32_t minPoolSize, uint32_t maxPoolSize = 0) = 0;
    virtual ManagedTexture::Holder GetTextureFromPool(const std::string& poolName) = 0;
    virtual bool CreateGridTexturePool(const std::string& name, const Vec2<int32_t>& textureSize, ImDataType dataType, const Vec2<int32_t>& gridSize, uint32_t minPoolSize, uint32_t maxPoolSize = 0) = 0;
    virtual ManagedTexture::Holder GetGridTextureFromPool(const std::string& poolName) = 0;

    virtual bool GetTexturePoolAttributes(const std::string& poolName, Vec2<int32_t>& textureSize, ImDataType& dataType) = 0;
    virtual void SetUiThread(const std::thread::id& threadId) = 0;
    virtual bool UpdateTextureState() = 0;  // run this method in UI thread
    virtual void Release() = 0;
    virtual bool IsTextureFrom(const std::string& poolName, ManagedTexture::Holder hTx) = 0;

    virtual std::string GetError() const = 0;
    virtual void SetLogLevel(Logger::Level l) = 0;

};
MEDIACORE_API std::ostream& operator<<(std::ostream& os, const TextureManager* pTxMgr);
}